// Outil hors plugin : tests audio de SYNTHETIKA + captures PNG de la vraie fenetre (meme code que le plugin).
#include "BinaryData.h"
#include "PluginProcessor.h"
#include "PluginEditor.h"
using namespace juce;

static void pump (int n) { for (int i = 0; i < n; ++i) MessageManager::getInstance()->runDispatchLoopUntil (20); }

// guitare synthetique (Karplus-Strong) : suite de notes (Hz, duree s), avec un glissando entre deux notes
struct Pluck
{
    std::vector<float> line; int pos = 0; float last = 0;
    void start (double f, double sr)
    {
        line.assign ((size_t) jmax (2, (int) std::round (sr / f)), 0.0f);
        Random r (int (f * 1000));
        for (auto& v : line) v = r.nextFloat() * 2 - 1;
        pos = 0; last = 0;
    }
    float next()
    {
        if (line.empty()) return 0;
        const float y = line[(size_t) pos];
        const int nx = (pos + 1) % (int) line.size();
        line[(size_t) pos] = 0.4985f * (y + line[(size_t) nx]);
        pos = nx; return y;
    }
};

static std::vector<float> guitarPhrase (double sr, std::vector<std::pair<double, double>> notes)
{
    std::vector<float> out; Pluck p;
    for (auto [f, d] : notes)
    {
        p.start (f, sr);
        const int n = (int) (d * sr);
        for (int i = 0; i < n; ++i) out.push_back (0.35f * p.next());
    }
    return out;
}

// hauteur mesuree sur la sortie (autocorrelation simple, fenetre au milieu de chaque note)
static double measure (const std::vector<float>& x, int start, int len, double sr)
{
    double best = 0; int bestLag = 0;
    for (int lag = (int) (sr / 1200); lag < (int) (sr / 40); ++lag)
    {
        double s = 0, e1 = 0, e2 = 0;
        for (int i = 0; i < len; ++i) { const double a = x[(size_t) (start + i)], b = x[(size_t) (start + i + lag)]; s += a * b; e1 += a * a; e2 += b * b; }
        const double c = s / std::sqrt (e1 * e2 + 1e-20);
        if (c > best + 0.02 || (c > 0.9 && bestLag == 0)) { best = c; bestLag = lag; if (c > 0.97) break; }
    }
    return bestLag > 0 ? sr / bestLag : 0;
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
    const std::vector<std::pair<double, double>> notes = { { 98.0, 0.6 }, { 82.41, 0.6 }, { 146.83, 0.6 }, { 196.0, 0.6 }, { 49.0, 0.8 }, { 329.63, 0.6 }, { 659.26, 0.6 }, { 987.77, 0.6 } };
    const auto phrase = guitarPhrase (sr, notes);
    const int total = (int) phrase.size();

    const char* engines[5] = { "Deep", "Vintage", "Solo", "Strings", "Harmony" };
    for (int e = 0; e < 5; ++e)
    {
        SynthetikaProcessor proc;
        proc.prepareToPlay (sr, 256);
        proc.applyFactoryPreset (1);                       // Soklo Bass : synthe seul, sans effet
        proc.setParamValue ("engine", (float) e);
        proc.setParamValue ("verbOn", 0.0f); proc.setParamValue ("modOn", 0.0f); proc.setParamValue ("distOn", 0.0f);
        AudioBuffer<float> all (2, total);
        bool bad = false; double peak = 0;
        std::vector<float> tracked;
        for (int pos = 0; pos < total; pos += 256)
        {
            const int n = jmin (256, total - pos);
            AudioBuffer<float> b (2, n); MidiBuffer m;
            for (int i = 0; i < n; ++i) { b.setSample (0, i, phrase[(size_t) (pos + i)]); b.setSample (1, i, phrase[(size_t) (pos + i)]); }
            proc.processBlock (b, m);
            for (int i = 0; i < n; ++i) { const float v = b.getSample (0, i); if (! std::isfinite (v)) bad = true; peak = jmax (peak, (double) std::abs (v)); }
            all.copyFrom (0, pos, b, 0, 0, n); all.copyFrom (1, pos, b, 1, 0, n);
            tracked.push_back (proc.trackedHz.load());
        }
        if (e == 0)
        {
            std::printf ("suivi (Hz reel du pincement -> Hz suivi, ecart en cents) : ");
            double t0 = 0;
            for (auto [f, d] : notes)
            {
                const double real = sr / (std::round (sr / f) + 0.5);
                const int blk = (int) ((t0 + d * 0.5) * sr / 256);
                const double tr = tracked[(size_t) jmin ((int) tracked.size() - 1, blk)];
                std::printf ("%.1f->%.1f (%+.0f) ", real, tr, 1200 * std::log2 (tr / real));
                t0 += d;
            }
            std::printf ("\n");
        }
        std::vector<float> mono ((size_t) total);
        for (int i = 0; i < total; ++i) mono[(size_t) i] = all.getSample (0, i);
        double ss = 0; for (int i = 0; i < total; ++i) ss += (double) all.getSample (0, i) * all.getSample (0, i);
        double si = 0; for (auto v : phrase) si += (double) v * v;
        std::printf ("%-8s rms %.1f dB (guitare %.1f) crete %.2f%s | hauteurs : ", engines[e], 10 * std::log10 (ss / total), 10 * std::log10 (si / total), peak, bad ? "  !! NaN" : "");
        double t = 0;
        for (auto [f, d] : notes)
        {
            const int st = (int) ((t + d * 0.45) * sr);
            const double fm = measure (mono, st, (int) (0.05 * sr), sr);
            std::printf ("%.0f>%.0f ", f, fm);
            t += d;
        }
        std::printf ("\n");
        writeWav (File (out).getChildFile (String ("engine_") + engines[e] + ".wav"), all, sr);
    }

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
    {
        AudioBuffer<float> in (1, total);
        for (int i = 0; i < total; ++i) in.setSample (0, i, phrase[(size_t) i]);
        writeWav (File (out).getChildFile ("guitare_entree.wav"), in, sr);
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
    }
    return 0;
}
