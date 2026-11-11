// Outil hors plugin : tests audio de SYNTHETIKA + captures PNG de la vraie fenetre (meme code que le plugin).
#include "BinaryData.h"
#include "PluginProcessor.h"
#include "PluginEditor.h"
using namespace juce;

static void pump (int n) { for (int i = 0; i < n; ++i) MessageManager::getInstance()->runDispatchLoopUntil (20); }

// guitare synthetique (Karplus-Strong a retard fractionnaire) : notes pincees, bends, vibrato, legato
struct Seg { double f0, f1, dur; bool pluck; double vibCents = 0; };
struct Played { std::vector<float> x; std::vector<float> truth; std::vector<int> segStart; };

static Played play (double sr, const std::vector<Seg>& segs, unsigned seed = 7)
{
    Played P; std::vector<float> buf (8192, 0.0f); int w = 0; float prev = 0; Random rnd ((int) seed);
    double t = 0;
    for (auto& s : segs)
    {
        const int n = (int) (s.dur * sr);
        P.segStart.push_back ((int) P.x.size());
        if (s.pluck)
        {
            const int L = (int) std::round (sr / s.f0);
            float lp = 0;
            for (int i = 0; i < L; ++i) { lp += 0.55f * ((rnd.nextFloat() * 2 - 1) - lp); buf[(size_t) ((w - L + i + 8192) & 8191)] = lp * 1.6f; }
        }
        for (int i = 0; i < n; ++i, t += 1.0 / sr)
        {
            const double u = (double) i / n;
            double f = s.f0 * std::pow (s.f1 / s.f0, jmin (1.0, u * 1.6));
            f *= std::pow (2.0, s.vibCents * std::sin (2 * MathConstants<double>::pi * 5.5 * t) / 1200.0);
            const double D = sr / f - 0.5;
            const double rp = (double) w - D; int i0 = (int) std::floor (rp); const float fr = (float) (rp - i0);
            const float a = buf[(size_t) (i0 & 8191)], b = buf[(size_t) ((i0 + 1) & 8191)];
            const float y = a + fr * (b - a);
            const float v = 0.4993f * (y + prev); prev = y;
            buf[(size_t) w] = v; w = (w + 1) & 8191;
            P.x.push_back (0.30f * y); P.truth.push_back ((float) f);
        }
    }
    return P;
}


static void writeWav (const File& f, const AudioBuffer<float>& b, double sr)
{
    f.deleteFile();
    WavAudioFormat wav;
    std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (new FileOutputStream (f), sr, (unsigned) b.getNumChannels(), 24, {}, 0));
    if (w) w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const String out = argc > 1 ? argv[1] : ".";
    const double sr = 48000.0;
    // 1) registres : basse 5 cordes (Si grave), 8 cordes (Fa#), 7 cordes (Si), guitare 6 cordes jusqu'a l'aigu
    const std::vector<Seg> range = { { 30.87, 30.87, 1.0, true }, { 41.20, 41.20, 0.8, true }, { 46.25, 46.25, 0.8, true }, { 61.74, 61.74, 0.6, true },
                                     { 82.41, 82.41, 0.5, true }, { 110.0, 110.0, 0.5, true }, { 196.0, 196.0, 0.5, true }, { 329.63, 329.63, 0.5, true },
                                     { 659.26, 659.26, 0.5, true }, { 987.77, 987.77, 0.5, true } };
    // 2) jeu : bend d'un ton et retour, vibrato, legato rapide (doubles croches a 150 bpm), slide, chug grave
    std::vector<Seg> lead = { { 196.0, 196.0, 0.25, true }, { 196.0, 220.0, 0.35, false }, { 220.0, 220.0, 0.30, false, 25 }, { 220.0, 196.0, 0.30, false },
                              { 293.66, 293.66, 0.5, true, 35 } };
    for (double f : { 146.83, 164.81, 174.61, 196.0, 220.0, 196.0, 174.61, 164.81, 146.83, 130.81, 123.47, 110.0 }) lead.push_back ({ f, f, 0.10, true });
    lead.push_back ({ 82.41, 82.41, 0.3, true }); lead.push_back ({ 82.41, 110.0, 0.25, false }); lead.push_back ({ 110.0, 110.0, 0.3, false });
    for (int k = 0; k < 6; ++k) lead.push_back ({ 61.74, 61.74, 0.11, true });
    lead.push_back ({ 41.2, 41.2, 0.4, true }); lead.push_back ({ 41.2, 46.25, 0.3, false }); lead.push_back ({ 46.25, 46.25, 0.4, false, 20 });

    const char* engines[5] = { "Deep", "Vintage", "Solo", "Strings", "Harmony" };
    auto runTest = [&] (const char* name, const std::vector<Seg>& segs)
    {
        const auto P = play (sr, segs);
        const int total = (int) P.x.size();
        {
            AudioBuffer<float> in (1, total);
            for (int i = 0; i < total; ++i) in.setSample (0, i, P.x[(size_t) i]);
            writeWav (File (out).getChildFile (String ("entree_") + name + ".wav"), in, sr);
        }
        for (int e = 0; e < 5; ++e)
        {
            SynthetikaProcessor proc;
            proc.prepareToPlay (sr, 128);
            proc.applyFactoryPreset (1);
            proc.setParamValue ("engine", (float) e);
            proc.setParamValue ("verbOn", 0.0f); proc.setParamValue ("modOn", 0.0f); proc.setParamValue ("distOn", 0.0f);
            AudioBuffer<float> all (2, total);
            double peak = 0, ss = 0, si = 0; bool bad = false;
            int checked = 0, wrong = 0, silent = 0; double maxErr = 0;
            std::vector<int> segWrong (segs.size(), 0);
            FILE* dbg = e == 0 ? std::fopen ((out + "/suivi_" + name + ".txt").toRawUTF8(), "w") : nullptr;
            for (int pos = 0; pos < total; pos += 128)
            {
                const int n = jmin (128, total - pos);
                AudioBuffer<float> b (2, n); MidiBuffer m;
                for (int i = 0; i < n; ++i) { b.setSample (0, i, P.x[(size_t) (pos + i)]); b.setSample (1, i, P.x[(size_t) (pos + i)]); si += P.x[(size_t) (pos + i)] * P.x[(size_t) (pos + i)]; }
                proc.processBlock (b, m);
                for (int i = 0; i < n; ++i) { const float v = b.getSample (0, i); if (! std::isfinite (v)) bad = true; peak = jmax (peak, (double) std::abs (v)); ss += v * v; }
                all.copyFrom (0, pos, b, 0, 0, n); all.copyFrom (1, pos, b, 1, 0, n);
                const int at = pos + n - 1;
                int sIdx = 0; for (size_t k = 0; k < P.segStart.size(); ++k) if (P.segStart[k] <= at) sIdx = (int) k;
                int lastPluck = sIdx; while (lastPluck > 0 && ! segs[(size_t) lastPluck].pluck) --lastPluck;
                const double truth = P.truth[(size_t) at];
                const int settle = (int) (sr * (2.5 / segs[(size_t) lastPluck].f0 + 0.020));
                if (at - P.segStart[(size_t) lastPluck] > settle)
                {
                    ++checked;
                    const double tr = proc.trackedHz.load();
                    if (dbg) std::fprintf (dbg, "%.4f %d %.2f %.2f\n", at / sr, sIdx, truth, tr);
                    if (tr <= 0) { ++silent; ++segWrong[(size_t) sIdx]; }
                    else { const double c = std::abs (1200 * std::log2 (tr / truth)); maxErr = jmax (maxErr, c); if (c > 30) { ++wrong; ++segWrong[(size_t) sIdx]; } }
                }
            }
            std::printf ("%-8s %-6s rms %.1f dB (entree %.1f) crete %.2f%s | %d controles : faux %d, decroche %d, ecart max %.0f c | segments en defaut :",
                         engines[e], name, 10 * std::log10 (ss / total + 1e-20), 10 * std::log10 (si / total + 1e-20), peak, bad ? " NaN!" : "", checked, wrong, silent, maxErr);
            for (size_t k = 0; k < segs.size(); ++k) if (segWrong[k]) std::printf (" %zu(%.0fHz)x%d", k, segs[k].f0, segWrong[k]);
            std::printf ("\n");
            if (dbg) std::fclose (dbg);
            writeWav (File (out).getChildFile (String ("engine_") + engines[e] + "_" + name + ".wav"), all, sr);
        }
    };
    runTest ("range", range);
    runTest ("lead", lead);
    const auto phrase = play (sr, lead).x;
    const int total = (int) phrase.size();

    // presets d'usine : stabilite (NaN, niveau)
    for (int pi = 0; pi < SynthetikaProcessor::getNumFactoryPresets(); ++pi)
    {
        SynthetikaProcessor proc; proc.prepareToPlay (sr, 512); proc.applyFactoryPreset (pi);
        double sum = 0, peak = 0; long cnt = 0; bool bad = false;
        AudioBuffer<float> all (2, total);
        for (int pos = 0; pos < total; pos += 512)
        {
            const int n = jmin (512, total - pos);
            AudioBuffer<float> b (2, n); MidiBuffer m;
            for (int i = 0; i < n; ++i) { b.setSample (0, i, phrase[(size_t) (pos + i)]); b.setSample (1, i, phrase[(size_t) (pos + i)]); }
            proc.processBlock (b, m);
            for (int i = 0; i < n; ++i) { const float v = b.getSample (0, i); if (! std::isfinite (v)) bad = true; sum += v * v; ++cnt; peak = jmax (peak, (double) std::abs (v)); }
            all.copyFrom (0, pos, b, 0, 0, n); all.copyFrom (1, pos, b, 1, 0, n);
        }
        std::printf ("preset %-18s rms %.1f dB crete %.2f%s\n", SynthetikaProcessor::getFactoryPreset (pi).name, 10 * std::log10 (sum / cnt + 1e-20), peak, bad ? " !! NaN" : "");
        writeWav (File (out).getChildFile ("preset_" + String (pi + 1).paddedLeft ('0', 2) + ".wav"), all, sr);
    }
    // captures de la fenetre
    {
        SynthetikaProcessor proc; proc.prepareToPlay (sr, 512);
        proc.applyFactoryPreset (2);
        std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
        auto* se = dynamic_cast<SynthetikaEditor*> (ed.get());
        for (int h : { SynthetikaEditor::sizeHeight (0), SynthetikaEditor::sizeHeight (2) })
        {
            se->setScaledHeight (h);
            se->forceRenderScale (2.0f);
            pump (30);
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
            PNGImageFormat png; FileOutputStream os (File (out).getChildFile ("capture_" + String (h) + ".png"));
            os.setPosition (0); os.truncate(); png.writeImageToStream (img, os);
            std::printf ("capture %d x %d\n", img.getWidth(), img.getHeight());
        }
        proc.setParamValue ("footswitch", 0.0f);
        se->setScaledHeight (SynthetikaEditor::sizeHeight (0)); se->forceRenderScale (2.0f);
        pump (40);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        PNGImageFormat png; FileOutputStream os (File (out).getChildFile ("capture_bypass.png"));
        os.setPosition (0); os.truncate(); png.writeImageToStream (img, os);
        ed.reset();
        // la fenetre rouverte doit garder la taille choisie
        std::unique_ptr<AudioProcessorEditor> ed2 (proc.createEditor());
        dynamic_cast<SynthetikaEditor*> (ed2.get())->setScaledHeight (SynthetikaEditor::sizeHeight (2));
        ed2.reset();
        std::unique_ptr<AudioProcessorEditor> ed3 (proc.createEditor());
        std::printf ("reouverture : hauteur %d (attendu %d)\n", ed3->getHeight(), SynthetikaEditor::sizeHeight (2));
        ed3.reset();
    }
    return 0;
}
