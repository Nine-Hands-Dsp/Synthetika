#pragma once
// SYNTHETIKA (NINE HANDS | DSP) - moteur audio : synthe guitare monophonique dans l'esprit du Soklo GS-5 (Audio Hertz).
//   Entree -> [chemin sec] ------------------------------------------------------------\
//          -> [synthe : suivi de hauteur YIN + 5 moteurs + modulation] -> distorsion PRE  -> MIX global -> OUTPUT
//             -> bitcrush / ring mod / gated reverb (ordre reglable) -> distorsion POST --/
// Mesures faites sur les extraits de Soklo fournis par l'utilisateur (nov. 2026) :
//   Deep  : onde quasi carree (3e harmonique ~ -10 dB, 5e ~ -15, pairs ~ -25), fondamentale forte, pas de sous-octave,
//           attaque ~80 ms, glissements continus qui suivent les slides ;
//   Harmony : quinte + octave (+7 / +12) par defaut ;
//   stereo : voix desaccordees + reverb a porte : correlation G/D qui baisse avec la frequence (0,65 a 150-600 Hz, ~0 au-dessus de 3 kHz).
#include <cmath>
#include <array>
#include <vector>
#include <algorithm>
#include <cstdint>

namespace synthetika
{
constexpr float kPi = 3.14159265358979f, kTwoPi = 6.28318530717959f;

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }

// ------------------------------------------------------------------ petits filtres
struct OnePole        // passe-bas 1 pole
{
    float a = 0.0f, z = 0.0f;
    void setHz (float hz, float sr) { a = 1.0f - std::exp (-kTwoPi * std::min (hz, 0.45f * sr) / sr); }
    float lp (float x) { z += a * (x - z); return z; }
    float hp (float x) { return x - lp (x); }
    void reset() { z = 0.0f; }
};

struct DcBlock { float x1 = 0, y1 = 0, r = 0.995f; float run (float x) { const float y = x - x1 + r * y1; x1 = x; y1 = y; return y; } void reset() { x1 = y1 = 0; } };

struct Svf            // filtre a variables d'etat (TPT, Zavalishin) : passe-bas / passe-haut
{
    float g = 0.1f, k = 1.4f, ic1 = 0, ic2 = 0;
    void set (float hz, float q, float sr)
    {
        g = std::tan (kPi * std::clamp (hz, 10.0f, 0.46f * sr) / sr);
        k = 1.0f / std::max (0.3f, q);
    }
    float lp (float v0)
    {
        const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const float v3 = v0 - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
        return v2;
    }
    float hp (float v0)
    {
        const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const float v3 = v0 - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
        return v0 - k * v1 - v2;
    }
    void reset() { ic1 = ic2 = 0; }
};

struct Ladder         // passe-bas 4 poles facon transistor (moteur VINTAGE), saturation douce dans la boucle
{
    float s[4] {}, g = 0.1f, res = 0.2f;
    void set (float hz, float r, float sr) { g = std::tan (kPi * std::clamp (hz, 20.0f, 0.45f * sr) / sr); g = g / (1 + g); res = r; }
    float run (float x)
    {
        const float fb = res * 4.0f * s[3];
        float u = std::tanh (x - fb);
        for (int i = 0; i < 4; ++i) { const float v = g * (u - s[i]); const float y = v + s[i]; s[i] = y + v; u = y; }
        return u;
    }
    void reset() { for (auto& v : s) v = 0; }
};

// ------------------------------------------------------------------ oscillateurs PolyBLEP (sans repliement audible)
inline float blep (float t, float dt)
{
    if (t < dt)       { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt){ t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

struct Osc
{
    float ph = 0.0f;
    // avance et renvoie scie, carre (rapport cyclique duty) dans s, q
    void tick (float dt, float duty, float& saw, float& sq)
    {
        saw = 2.0f * ph - 1.0f - blep (ph, dt);
        float p2 = ph + 1.0f - duty; if (p2 >= 1.0f) p2 -= 1.0f;
        sq = (ph < duty ? 1.0f : -1.0f) + blep (ph, dt) - blep (p2, dt);
        ph += dt; if (ph >= 1.0f) ph -= 1.0f;
    }
};
// ------------------------------------------------------------------ suivi de hauteur YIN monophonique (v1.1)
// Signal decime vers ~11-12 kHz ; decalage jusqu'a 28 Hz (basse 5 cordes en Si, guitares 7 et 8 cordes) ;
// fenetre adaptee a la note en cours (courte dans l'aigu : reponse rapide, longue dans le grave : stable) ;
// calcul arrete des que le creux de la periode est trouve ; affinage a pleine resolution autour de la periode.
class PitchTracker
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        dec = std::max (1, (int) std::lround (sampleRate / 11025.0));
        dsr = sr / (float) dec;
        tauMax = std::min (kTauCap, (int) (dsr / 28.0f));
        aa1.set (dsr * 0.42f, 0.54f, sr); aa2.set (dsr * 0.42f, 1.31f, sr);
        hp.set (20.0f, 0.7f, sr);
        buf.assign (kBuf, 0.0f); w = 0; cnt = 0; hopCnt = 0;
        fbuf.assign (kFBuf, 0.0f); fw = 0;
        freq = 0; conf = 0; lastTau = 0;
        d.assign ((size_t) kTauCap + 2, 0.0f);
    }
    bool push (float x)
    {
        const float xh = hp.hp (x);
        fbuf[(size_t) fw] = xh; fw = (fw + 1) & (kFBuf - 1);
        x = aa2.lp (aa1.lp (xh));
        if (++cnt < dec) return false;
        cnt = 0;
        buf[(size_t) w] = x; w = (w + 1) & (kBuf - 1);
        // pas d'analyse : 16 echantillons decimes (~1,5 ms), plus long dans le grave (~1/6 de periode, 4 ms au plus)
        const int hop = shortWin || lastTau <= 96 ? kHop : std::min (48, lastTau / 6);
        if (++hopCnt < hop) return false;
        hopCnt = 0;
        analyse();
        return true;
    }
    void resetWindow() { shortWin = true; shortHold = 8; }    // nouvelle attaque : fenetre courte, l'ancienne note sort vite de l'analyse
    float frequency() const { return freq; }
    float confidence() const { return conf; }

private:
    static constexpr int kBuf = 2048, kTauCap = 470, kHop = 16, kFBuf = 8192;

    void analyse()
    {
        if (shortHold > 0) --shortHold;
        // fenetre : ~1,7 periode de la note en cours (96 a 400 echantillons decimes)
        const int win = shortWin ? 96 : (lastTau > 0 ? std::clamp ((int) ((lastTau > 100 ? 1.1f : 1.4f) * (float) lastTau), 96, 400) : 300);
        const int tauMin = std::max (2, (int) (dsr / 1500.0f));
        const int span = win + tauMax;
        const int start = (w - span + kBuf) & (kBuf - 1);
        float frame[400 + kTauCap];
        float energy = 0.0f;
        for (int i = 0; i < span; ++i) frame[i] = buf[(size_t) ((start + i) & (kBuf - 1))];
        // note grave : moyenne glissante (~1/6 de periode) qui ne garde que les premiers harmoniques ;
        // les slides et bends sur les cordes graves (basse, 7 et 8 cordes) restent ainsi bien suivis
        if (! shortWin && lastTau > 100)
        {
            const int M = lastTau / 6;
            float acc = 0.0f, hist[128] {}; int h = 0;
            for (int i = 0; i < span; ++i)
            {
                acc += frame[i] - hist[h]; hist[h] = frame[i]; h = (h + 1) % M;
                frame[i] = acc / (float) M;
            }
        }
        const float* now = frame + tauMax;
        for (int j = 0; j < win; ++j) energy += now[j] * now[j];
        if (energy < 2.0e-8f * (float) win) { conf = 0.0f; return; }

        float run = 0.0f, best = 2.0f; int bestTau = -1;
        bool inDip = false;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            const float* past = now - tau;
            float s = 0.0f;
            for (int j = 0; j < win; ++j) { const float e = now[j] - past[j]; s += e * e; }
            run += s;
            const float v = run > 0.0f ? s * (float) tau / run : 1.0f;
            d[(size_t) tau] = v;
            if (tau < tauMin) continue;
            if (v < 0.13f) { inDip = true; if (v < best) { best = v; bestTau = tau; } }
            else if (inDip) break;                                        // creux de la periode depasse : on s'arrete
            else if (v < best) { best = v; bestTau = tau; }
        }
        if (bestTau < tauMin || best > 0.40f) { conf = std::max (0.0f, 1.0f - best) * 0.4f; return; }
        // sous-harmonique ? (corde grave au pincement : le creux d'une periode double peut etre plus profond)
        // on prend la plus petite periode tau/k qui forme elle aussi un creux net
        for (int k = 4; k >= 2; --k)
        {
            const int c0 = (int) std::lround ((float) bestTau / (float) k);
            if (c0 - 2 < tauMin) continue;
            int m = c0; for (int t2 = c0 - 2; t2 <= c0 + 2; ++t2) if (d[(size_t) t2] < d[(size_t) m]) m = t2;
            if (m > c0 - 2 && m < c0 + 2 && d[(size_t) m] < std::min (0.30f, best + 0.18f)) { best = d[(size_t) m]; bestTau = m; break; }
        }
        float t = (float) bestTau;
        if (bestTau > 1 && bestTau < tauMax)
        {
            const float a = d[(size_t) bestTau - 1], b = d[(size_t) bestTau], c = d[(size_t) bestTau + 1];
            const float den = a - 2 * b + c;
            if (std::abs (den) > 1e-9f) t += std::clamp (0.5f * (a - c) / den, -0.5f, 0.5f);
        }
        if (best < 0.20f) { lastTau = bestTau; if (shortHold == 0) shortWin = false; }   // seule une periode nette regle la fenetre
        freq = refine (t * (float) dec);
        conf = 1.0f - best;
    }

    // affinage a pleine resolution : difference normalisee autour de la periode grossiere, minimum parabolique
    float refine (float lag0)
    {
        const int L0 = (int) std::lround (lag0), span = std::min (dec * 2 + 1, 31);   // recherche symetrique (2 x 31 + 1 <= 63)
        const int win = std::clamp (2 * L0, 256, 2048);
        if (L0 + span + win + 2 >= kFBuf) return sr / lag0;
        float dv[64]; const int n = 2 * span + 1;
        int bestI = -1; float best = 1e30f;
        for (int k = 0; k < n; ++k)
        {
            const int L = L0 - span + k;
            if (L < 2) { dv[k] = 1e30f; continue; }
            float s = 0, e = 0;
            for (int j = 0; j < win; ++j)
            {
                const float a = fbuf[(size_t) ((fw - 1 - j) & (kFBuf - 1))], b = fbuf[(size_t) ((fw - 1 - j - L) & (kFBuf - 1))];
                s += (a - b) * (a - b); e += a * a + b * b;
            }
            dv[k] = e > 0 ? s / e : 1.0f;
            if (dv[k] < best) { best = dv[k]; bestI = k; }
        }
        if (bestI <= 0 || bestI >= n - 1) return sr / lag0;
        const float a = dv[bestI - 1], b = dv[bestI], c = dv[bestI + 1], den = a - 2 * b + c;
        const float L = (float) (L0 - span + bestI) + (std::abs (den) > 1e-12f ? std::clamp (0.5f * (a - c) / den, -0.5f, 0.5f) : 0.0f);
        return sr / L;
    }

    float sr = 48000, dsr = 12000;
    int dec = 4, cnt = 0, hopCnt = 0, w = 0, tauMax = 400, lastTau = 0;
    bool shortWin = false; int shortHold = 0;
    Svf aa1, aa2, hp;
    std::vector<float> buf, d, fbuf; int fw = 0;
    float freq = 0, conf = 0;
};

struct Params
{
    float inGain = 1, outGain = 1, mix = 1;
    bool  synthOn = true; float synthMix = 1; int engine = 0; int int1 = 7; int int2 = 12; bool int2On = true;
    bool  distOn = false; float distGain = 0.4f; int distMode = 0; bool distPre = true, distPost = false;
    bool  modOn = false; float modDepth = 0.3f, modMix = 0.5f;
    bool  ringOn = false; float ringHz = 440, ringMix = 0.5f;
    bool  crushOn = false; float crushHz = 44100, crushBits = 24;
    bool  verbOn = false; float verbMs = 300, verbMix = 0.3f;
    int   order = 0;          // 0 C R V, 1 C V R, 2 R C V, 3 R V C, 4 V C R, 5 V R C
    bool  active = true;
};

inline void orderOf (int order, int out[3])
{
    static const int perms[6][3] = { { 0, 1, 2 }, { 0, 2, 1 }, { 1, 0, 2 }, { 1, 2, 0 }, { 2, 0, 1 }, { 2, 1, 0 } };
    const auto& p = perms[std::clamp (order, 0, 5)];
    out[0] = p[0]; out[1] = p[1]; out[2] = p[2];
}
inline int orderIndex (const int o[3])
{
    for (int i = 0; i < 6; ++i) { int t[3]; orderOf (i, t); if (t[0] == o[0] && t[1] == o[1] && t[2] == o[2]) return i; }
    return 0;
}
// ------------------------------------------------------------------ moteur complet
class Engine
{
public:
    void prepare (double sampleRate, int maxBlock)
    {
        sr = (float) sampleRate;
        tracker.prepare (sampleRate);
        for (auto& v : voices) v.reset ((float) rand01());
        envFast = envSlow = 0; amp = 0; fEnv = 0; logF = std::log2 (110.0f); logFTarget = logF; haveNote = false;
        holdN = 0; waitN = 0; onsetN = 0; candVotes = 0; candLog = 0;
        inHp.set (25.0f, 0.7f, sr); onHp.set (1800.0f, 0.7f, sr); hfFast = hfSlow = 0;
        const int maxD = (int) (sr * 1.15f) + 8;
        for (int c = 0; c < 2; ++c)
        {
            verbBuf[c].assign ((size_t) maxD + 1, 0.0f);
            for (int i = 0; i < 4; ++i) ap[c][i].prepare ((int) (sr * apMs[c][i] * 0.001f));
            vHp[c].set (180.0f, 0.7f, sr); vLp[c].set (7000.0f, 0.6f, sr);
        }
        verbW = 0;
        std::uint32_t seed = 0x5eed1234u;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < kTaps; ++i)
            {
                seed = seed * 1664525u + 1013904223u;
                tapPos[c][i] = ((float) i + 0.15f + 0.7f * (float) ((seed >> 8) & 0xffff) / 65535.0f) / (float) kTaps;
                seed = seed * 1664525u + 1013904223u;
                tapLfoPh[c][i] = (float) ((seed >> 8) & 0xffff) / 65535.0f;
            }
        crushPh = 0; crushHold[0] = crushHold[1] = 0; ringPh = 0;
        for (auto& dd : dcPre) dd.reset();
        for (auto& l : lpPre) l.reset();
        prepareOversampling();
        curEng = wantEng = 0; engXf = 1.0f;
        sMix.reset (sr, 0.03f, 1.0f); sSynMix.reset (sr, 0.03f, 1.0f); sIn.reset (sr, 0.03f, 1.0f); sOut.reset (sr, 0.03f, 1.0f);
        sDist.reset (sr, 0.03f, 0.4f); sModD.reset (sr, 0.05f, 0.3f); sModM.reset (sr, 0.03f, 0.5f);
        sRingHz.reset (sr, 0.03f, 440.f); sRingM.reset (sr, 0.03f, 0.5f); sCrHz.reset (sr, 0.03f, 44100.f); sBits.reset (sr, 0.03f, 24.f);
        sVerbMs.reset (sr, 0.08f, 300.f); sVerbM.reset (sr, 0.03f, 0.3f); sActive.reset (sr, 0.015f, 1.0f);
        for (auto& g : modOnG) g.reset (sr, 0.01f, 0.0f);
        for (auto& g : voiceGain) g.reset (sr, 0.02f, 1.0f);
        inPeak = outPeak = 0.0f;
        (void) maxBlock;
    }

    float inPeak = 0.0f, outPeak = 0.0f;
    int curEng = 0, wantEng = 0; float engXf = 1.0f;
    float trackedHz() const { return haveNote ? std::exp2 (logF) : 0.0f; }

    void process (float* const* io, int numCh, int n, const Params& p)
    {
        sMix.target (p.mix); sSynMix.target (p.synthMix); sIn.target (p.inGain); sOut.target (p.outGain);
        sDist.target (p.distGain); sModD.target (p.modDepth); sModM.target (p.modMix);
        sRingHz.target (p.ringHz); sRingM.target (p.ringMix); sCrHz.target (std::min (p.crushHz, 44100.0f)); sBits.target (p.crushBits);
        sVerbMs.target (p.verbMs); sVerbM.target (p.verbMix); sActive.target (p.active ? 1.0f : 0.0f);
        modOnG[0].target (p.modOn ? 1.0f : 0.0f);
        modOnG[1].target (p.synthOn ? 1.0f : 0.0f);
        if (p.verbOn && onG[3].want < 0.5f && onG[3].v <= 0.0f) clearVerb();     // rallumage apres coupure complete : pas d'echo fantome
        onG[0].set (p.distOn); onG[1].set (p.ringOn); onG[2].set (p.crushOn); onG[3].set (p.verbOn);
        voiceGain[0].target (1.0f);
        voiceGain[1].target (p.engine == 4 ? 1.0f : 0.0f);
        voiceGain[2].target (p.engine == 4 && p.int2On ? 1.0f : 0.0f);
        int ord[3]; orderOf (p.order, ord);
        wantEng = std::clamp (p.engine, 0, 4);
        const EngineSpec& es = spec (curEng);
        const float xfStep = 1.0f / (0.008f * sr);
        const float atkC = std::exp (-1.0f / (es.attack * sr)), relC = std::exp (-1.0f / (es.release * sr));
        const float fEnvC = std::exp (-1.0f / (es.filterDecay * sr));
        const float cutC = std::exp (-1.0f / ((curEng == 3 ? 0.015f : 0.005f) * sr));                 // nouvelle attaque : l'ancienne note se tait en ~5 ms
        const float glideC = 1.0f - std::exp (-1.0f / (0.0025f * sr));        // 2,5 ms : bends et vibrato suivis sans retard audible
        const float slowC = 1.0f - std::exp (-1.0f / (0.030f * sr));
        const float hfAtkC = 1.0f - std::exp (-1.0f / (0.025f * sr)), hfRelC = 1.0f - std::exp (-1.0f / (0.002f * sr));
        const int waitMax = (int) (0.110f * sr), holdMax = (int) (0.080f * sr), onsetMax = (int) (0.050f * sr), minWaitN = (int) (0.009f * sr);

        float* L = io[0]; float* R = numCh > 1 ? io[1] : nullptr;
        for (int i = 0; i < n; ++i)
        {
            const float rawL = L[i], rawR = R ? R[i] : L[i];
            const float active = sActive.next();
            const float gIn = sIn.next();
            const float xl = rawL * gIn, xr = rawR * gIn;
            const float x = 0.5f * (xl + xr);                                   // signal analyse (suivi, attaques)
            inPeak = std::max (inPeak * 0.9995f, std::max (std::abs (xl), std::abs (xr)));

            // ---- enveloppe de la corde et detection d'attaque
            const float ax = std::abs (inHp.hp (x));
            envFast += (ax > envFast ? 0.30f : 0.0012f) * (ax - envFast);
            envSlow += slowC * (envFast - envSlow);
            // attaque du mediator : bouffee d'aigus (> 1,8 kHz), visible meme quand la note precedente sonne encore fort
            const float ah = std::abs (onHp.hp (x));
            hfFast += (ah > hfFast ? 0.25f : 0.0025f) * (ah - hfFast);
            hfSlow += (hfFast < hfSlow ? hfRelC : hfAtkC) * (hfFast - hfSlow);          // plancher : suit les creux, monte lentement
            const bool pick = (envFast > 0.003f && envFast > 1.9f * envSlow + 0.0015f)
                           || (hfFast > 0.0012f && hfFast > 1.7f * hfSlow + 0.0006f);
            if (pick && onsetN == 0)
            {
                onsetN = onsetMax; waitN = waitMax; candVotes = 0;           // nouvelle note : on attend la nouvelle hauteur
                fEnv = 1.0f; tracker.resetWindow();
            }
            if (onsetN > 0) --onsetN;

            // ---- suivi de hauteur
            //  - bends, vibrato, slides : petits ecarts acceptes a chaque analyse (~1,5 ms), suivis en continu ;
            //  - nouvelle note (attaque) : on ignore l'analyse tant que l'ancienne note la remplit (9 ms), puis deux
            //    estimations concordantes et la note demarre directement a la bonne hauteur (pas de glissement) ;
            //  - legato (hammer-on, pull-off) : saut confirme par 3 analyses ; saut d'octave sans attaque : 30 ms.
            if (tracker.push (x))
            {
                const float f = tracker.frequency(), c = tracker.confidence();
                const bool fresh = onsetN == 0 || onsetMax - onsetN > minWaitN;
                const float lf = f > 1.0f ? std::log2 (f) : 0.0f;
                const float jump = std::abs (lf - logFTarget);
                const float cMin = haveNote && waitN == 0 && jump < 0.06f ? 0.45f : 0.62f;     // continuite (bend, slide) : plus tolerant
                if (fresh && f > 27.0f && f < 1600.0f && c > cMin && envFast > 0.0008f)
                {
                    // estimations concordantes : on suit le candidat s'il derive peu (slide), sinon on repart de zero
                    if (std::abs (lf - candLog) < 0.04f) ++candVotes; else candVotes = 1;
                    candLog = lf;
                    const bool newNote = ! haveNote || waitN > 0;
                    int need = 1;
                    if (newNote) need = 2;
                    else if (jump > 0.06f)
                    {
                        const bool octave = std::abs (jump - 1.0f) < 0.07f || std::abs (jump - 2.0f) < 0.07f;
                        need = ! octave ? 3 : (onsetN > 0 && lf > logFTarget ? 3 : 20);   // juste apres l'attaque : correction d'une octave trop basse
                    }
                    if (candVotes >= need)
                    {
                        if (newNote || jump > 0.06f) logF = lf;
                        logFTarget = lf; haveNote = true; holdN = holdMax; waitN = 0;
                    }
                }
            }
            if (waitN > 0 && --waitN == 0) {}                                  // pas de nouvelle hauteur en 30 ms : on garde l'ancienne
            if (holdN > 0) --holdN;
            logF += (logFTarget - logF) * glideC;

            // amplitude : suit l'attaque et le sustain de la corde ; muette tant que la nouvelle hauteur n'est pas connue
            const bool sounding = haveNote && (holdN > 0 || envFast > 0.0022f);
            const float target = (sounding && waitN == 0) ? envFast * 1.55f : 0.0f;
            amp = target > amp ? target + (amp - target) * atkC : target + (amp - target) * (waitN > 0 ? cutC : relC);
            fEnv *= fEnvC;

            // ---- synthe
            float synL = 0.0f, synR = 0.0f;
            const float synOn = modOnG[1].next();
            const float modG = modOnG[0].next();
            const float depth = sModD.next() * modG;
            const float mmRaw = sModM.next() * modG;
            float vg[3]; for (int v = 0; v < 3; ++v) vg[v] = voiceGain[(size_t) v].next();
            if (wantEng != curEng) { engXf -= xfStep; if (engXf <= 0.0f) { engXf = 0.0f; curEng = wantEng; for (auto& v : voices) v.clearFilters(); } }
            else if (engXf < 1.0f) engXf = std::min (1.0f, engXf + xfStep);
            const int eng = curEng;
            if (synOn > 0.0001f && haveNote && amp > 1.0e-6f)
            {
                const float det = std::exp2 (depth * 32.0f / 1200.0f);
                const float mm = mmRaw;                                         // suit l'interrupteur en fondu : pas de saut
                const float semis[3] = { 0.0f, (float) p.int1, (float) p.int2 };
                float c = 0, fl = 0, sh = 0;
                for (int v = 0; v < 3; ++v)
                {
                    if (vg[v] < 0.0001f) continue;
                    const float f = std::exp2 (logF + semis[v] / 12.0f);
                    float cc, ff, ss;
                    voices[(size_t) v].render (eng, f, det, mm > 0.0001f, sr, amp, fEnv, cc, ff, ss);
                    c += vg[v] * cc; fl += vg[v] * ff; sh += vg[v] * ss;
                }
                const float norm = eng == 4 ? (p.int2On ? 0.62f : 0.75f) : 1.0f;
                // copies identiques a la voix centrale (DEPTH a 0) : meme niveau qu'avec la modulation coupee
                const float comp = 1.0f + 0.40f * mm * std::min (1.0f, depth * 6.0f);   // copies qui battent : niveau moyen conserve
                synL = norm * comp * (c * (1.0f - 0.5f * mm) + fl * 0.5f * mm) * amp * synOn * engXf;
                synR = norm * comp * (c * (1.0f - 0.5f * mm) + sh * 0.5f * mm) * amp * synOn * engXf;
            }

            const float sm = sSynMix.next() * synOn;
            float l = xl * (1.0f - sm) + synL * sm;
            float r = xr * (1.0f - sm) + synR * sm;

            const float dg = sDist.next();
            const float dOn = onG[0].next();
            const float driveDb = 36.0f * dg, dGain = dbToGain (driveDb), dComp = 1.0f / (1.0f + 0.055f * driveDb);
            if (dOn > 0.0f && p.distPre) { const float yl = shape (l, dGain, dComp, p.distMode, 0), yr = shape (r, dGain, dComp, p.distMode, 1); l += (yl - l) * dOn; r += (yr - r) * dOn; }

            const float crHz = sCrHz.next(), bits = sBits.next(), rHz = sRingHz.next(), rM = sRingM.next(), vMs = sVerbMs.next(), vM = sVerbM.next();
            const float gRing = onG[1].next(), gCr = onG[2].next(), gVerb = onG[3].next();
            for (int k = 0; k < 3; ++k)
            {
                switch (ord[k])
                {
                    case 0: if (gCr > 0.0f)   { float a = l, b = r; crush (a, b, crHz, bits); l += (a - l) * gCr; r += (b - r) * gCr; } break;
                    case 1: if (gRing > 0.0f) { const float car = std::sin (ringPh * kTwoPi); const float m = rM * gRing; l = l * (1 - m) + l * car * m; r = r * (1 - m) + r * car * m; } break;
                    case 2: if (gVerb > 0.0f) { float wl, wr; verb (l, r, vMs, wl, wr); const float m = vM * gVerb; l = l * (1.0f - 0.5f * m) + wl * m; r = r * (1.0f - 0.5f * m) + wr * m; } break;
                    default: break;
                }
            }
            ringPh += rHz / sr; if (ringPh >= 1.0f) ringPh -= 1.0f;

            if (dOn > 0.0f && p.distPost) { const float yl = shape (l, dGain, dComp, p.distMode, 2), yr = shape (r, dGain, dComp, p.distMode, 3); l += (yl - l) * dOn; r += (yr - r) * dOn; }

            const float m = sMix.next(), go = sOut.next();
            float ol = (xl * (1.0f - m) + l * m) * go;
            float orr = (xr * (1.0f - m) + r * m) * go;
            ol = rawL + (ol - rawL) * active; orr = rawR + (orr - rawR) * active;            // bypass : entree rendue telle quelle
            if (! std::isfinite (ol)) ol = 0;
            if (! std::isfinite (orr)) orr = 0;
            outPeak = std::max (outPeak * 0.9995f, std::max (std::abs (ol), std::abs (orr)));
            if (R) { L[i] = ol; R[i] = orr; } else L[i] = 0.5f * (ol + orr);
        }
    }

private:
    // ------------------------------------------------------------------ moteurs : enveloppes
    struct EngineSpec { float attack, release, filterDecay; };
    static const EngineSpec& spec (int e)
    {
        static const EngineSpec s[5] = {
            { 0.012f, 0.040f, 0.090f },   // DEEP : franc, basse qui claque (attaque plus courte qu'en 1.0 : moins de bavure)
            { 0.006f, 0.060f, 0.220f },   // VINTAGE : filtre qui se referme sur chaque note (« wow » analogique)
            { 0.003f, 0.050f, 0.140f },   // SOLO : lead immediat
            { 0.160f, 0.300f, 0.600f },   // STRINGS : nappe lente
            { 0.008f, 0.050f, 0.150f },   // HARMONY
        };
        return s[std::clamp (e, 0, 4)];
    }

    // ------------------------------------------------------------------ voix : 5 timbres bien distincts
    struct Voice
    {
        std::array<Osc, 15> osc;          // 5 oscillateurs x (centre, bas, haut)
        Svf lp[3], hp[3]; Ladder lad[3];
        float lfo1 = 0, lfo2 = 0; unsigned tick[3] {}; bool copiesLive = false;
        void clearFilters() { for (int c = 0; c < 3; ++c) { lp[c].reset(); hp[c].reset(); lad[c].reset(); tick[c] = 0; } }
        void reset (float r)
        {
            for (size_t k = 0; k < osc.size(); ++k) osc[k].ph = std::fmod (r * (float) (k + 1) * 0.618f, 1.0f);
            for (int c = 0; c < 3; ++c) { lp[c].reset(); hp[c].reset(); lad[c].reset(); }
        }
        void render (int eng, float f, float det, bool copies, float sr, float env, float fEnv, float& c, float& lo, float& hi)
        {
            lfo1 += 0.83f / sr; if (lfo1 >= 1) lfo1 -= 1;
            lfo2 += 5.3f / sr;  if (lfo2 >= 1) lfo2 -= 1;
            if (! copies) copiesLive = false;
            else if (! copiesLive || det < 1.00002f)
            {
                // copies de modulation calees sur la voix centrale : DEPTH a 0 = son identique, aucun saut de niveau
                for (int k = 0; k < 5; ++k) osc[(size_t) (5 + k)].ph = osc[(size_t) (10 + k)].ph = osc[(size_t) k].ph;
                lp[1] = lp[2] = lp[0]; hp[1] = hp[2] = hp[0]; lad[1] = lad[2] = lad[0];
                copiesLive = true;
            }
            c = one (eng, 0, f, sr, env, fEnv);
            if (copies) { lo = one (eng, 1, f / det, sr, env, fEnv); hi = one (eng, 2, f * det, sr, env, fEnv); }
            else lo = hi = c;
        }
        float one (int eng, int copy, float f, float sr, float env, float fEnv)
        {
            const int b = copy * 5;
            const bool upd = (tick[copy]++ & 3u) == 0u;                       // filtres : coefficients mis a jour toutes les 4 echantillons
            const float dt = std::min (0.45f, f / sr);
            const float bright = std::min (1.0f, env * 5.0f);                // jeu plus fort = plus brillant
            float s0, q0, s1, q1, s2, q2, s3, q3;
            switch (eng)
            {
                default:
                case 0:   // DEEP : carre 48 % + sinus de renfort (mesure sur l'extrait de Soklo), petit claquement de filtre
                {
                    osc[(size_t) b].tick (dt, 0.48f, s0, q0);
                    const float sine = std::sin (osc[(size_t) b].ph * kTwoPi);
                    if (upd) lp[copy].set (std::min (3600.0f, f * 26.0f * (1.0f + 0.9f * fEnv)), 0.62f, sr);
                    return lp[copy].lp (0.80f * q0 + 0.36f * sine) * 0.45f;
                }
                case 1:   // VINTAGE : deux scies (+/-6 cents) + sous-octave carree, passe-bas 4 poles qui se referme sur chaque note
                {
                    osc[(size_t) b].tick (dt * 1.00347f, 0.5f, s0, q0);
                    osc[(size_t) b + 1].tick (dt * 0.99654f, 0.5f, s1, q1);
                    osc[(size_t) b + 2].tick (dt * 0.5f, 0.5f, s2, q2);
                    const float cut = f * (1.4f + 16.0f * fEnv + 5.0f * bright);
                    if (upd) lad[copy].set (std::min (cut, 12000.0f), 0.40f, sr);
                    return lad[copy].run (0.55f * s0 + 0.38f * s1 + 0.30f * q2) * 2.2f;
                }
                case 2:   // SOLO : impulsion a largeur modulee + scie, passe-bas resonant et saturation : lead qui tranche
                {
                    const float pw = 0.5f + 0.17f * std::sin (lfo1 * kTwoPi);
                    osc[(size_t) b].tick (dt, pw, s0, q0);
                    osc[(size_t) b + 1].tick (dt * 1.0029f, 0.5f, s1, q1);
                    if (upd) lp[copy].set (std::clamp (f * 11.0f * (1.0f + 0.9f * fEnv) * (0.7f + 0.6f * bright), 500.0f, 9500.0f), 2.2f, sr);
                    return std::tanh (2.2f * lp[copy].lp (0.55f * q0 + 0.45f * s1)) * 0.52f;
                }
                case 3:   // STRINGS : ensemble de trois scies a desaccord anime + octave, aere (passe-haut) et doux
                {
                    const float wob = 0.0035f * std::sin (lfo1 * kTwoPi) + 0.0012f * std::sin (lfo2 * kTwoPi);
                    osc[(size_t) b].tick (dt * (0.9940f + wob), 0.5f, s0, q0);
                    osc[(size_t) b + 1].tick (dt * (1.0f - wob * 0.7f), 0.5f, s1, q1);
                    osc[(size_t) b + 2].tick (dt * (1.0062f + wob * 0.5f), 0.5f, s2, q2);
                    osc[(size_t) b + 3].tick (std::min (0.45f, dt * 2.0f), 0.5f, s3, q3);
                    if (upd) { hp[copy].set (f * 0.9f, 0.7f, sr); lp[copy].set (std::min (7000.0f, f * 10.0f), 0.55f, sr); }
                    return lp[copy].lp (hp[copy].hp (0.36f * (s0 + s1 + s2) + 0.16f * s3)) * 1.4f;
                }
                case 4:   // HARMONY : scie + carre, passe-bas moyen (chaque intervalle est une voix de ce type)
                {
                    osc[(size_t) b].tick (dt, 0.5f, s0, q0);
                    if (upd) lp[copy].set (std::min (6000.0f, f * 13.0f * (1.0f + 0.5f * fEnv)), 0.75f, sr);
                    return lp[copy].lp (0.5f * q0 + 0.5f * s0) * 1.45f;
                }
            }
        }
    };

    // ------------------------------------------------------------------ distorsion (meme gain et mode avant / apres)
    // suréchantillonnage x4 : passe-bas Butterworth d'ordre 8 avant et apres la courbe (repliement repousse sous -60 dB)
    static float curve (float x, float d, int mode)
    {
        switch (mode)
        {
            default:
            case 0: return std::tanh (d * x * 1.2f);                                                         // SOFT : arrondi, facon lampe
            case 1: { const float v = d * x * 2.0f + 0.08f, a = v * v; return v / std::sqrt (std::cbrt (1.0f + a * a * a)) - 0.08f; } // HARD : ecretage franc
            case 2: { const float v = d * x * 1.3f; return std::tanh (v + 0.32f * v * v); }                  // RICH : harmoniques paires
            case 3: { const float v = d * x * 1.6f; return std::sin (v * 1.5708f); }                          // WAVEFOLD : repli sur soi
        }
    }
    float shape (float x, float d, float comp, int mode, int st)
    {
        auto& up = osUp[st]; auto& dn = osDn[st];
        float y = 0.0f;
        for (int k = 0; k < kOs; ++k)
        {
            float u = (k == 0 ? x * (float) kOs : 0.0f);
            for (auto& f : up) u = f.lp (u);
            float v = curve (u, d, mode);
            for (auto& f : dn) v = f.lp (v);
            if (k == 0) y = v;                                                  // on garde un echantillon sur 4
        }
        y = dcPre[st].run (y);
        return lpPre[st].lp (y) * comp;
    }
    static constexpr int kOs = 4;
    void prepareOversampling()
    {
        static const float q[4] = { 0.5098f, 0.6013f, 0.9000f, 2.5629f };   // Butterworth ordre 8
        const float fc = std::min (19000.0f, 0.40f * sr);
        for (int st = 0; st < 4; ++st)
            for (int i = 0; i < 4; ++i)
            {
                osUp[st][i].set (fc, q[i], sr * kOs); osUp[st][i].reset();
                osDn[st][i].set (fc, q[i], sr * kOs); osDn[st][i].reset();
            }
        for (auto& l : lpPre) l.setHz (11000.0f, sr);
    }
    Svf osUp[4][4], osDn[4][4];

    // ------------------------------------------------------------------ bitcrush : frequence plafonnee a 44,1 kHz, resolution 1-24 bits
    void crush (float& l, float& r, float hz, float bits)
    {
        crushPh += std::min (1.0f, hz / sr);
        if (crushPh >= 1.0f) { crushPh -= 1.0f; crushHold[0] = l; crushHold[1] = r; }
        l = crushHold[0]; r = crushHold[1];
        if (bits < 23.9f)
        {
            const float q = std::exp2 (bits - 1.0f);
            l = std::round (l * q) / q; r = std::round (r * q) / q;
        }
    }

    // ------------------------------------------------------------------ reverb a porte (non lineaire) :
    // multi-prises a amplitude constante sur toute la duree puis coupure franche (rampe de 6 %),
    // diffusion par passe-tout differents a gauche et a droite, prises legerement modulees, filtres.
    struct Allpass
    {
        std::vector<float> b; int i = 0; float g = 0.62f;
        void prepare (int n) { b.assign ((size_t) std::max (1, n), 0.0f); i = 0; }
        float run (float x) { const float d = b[(size_t) i]; const float y = -g * x + d; b[(size_t) i] = x + g * y; if (++i >= (int) b.size()) i = 0; return y; }
    };
    static constexpr int kTaps = 56;
    void verb (float l, float r, float ms, float& wl, float& wr)
    {
        const int size = (int) verbBuf[0].size();
        float in[2] = { l, r };
        for (int c = 0; c < 2; ++c)
        {
            float v = in[c];
            for (auto& a : ap[c]) v = a.run (v);
            verbBuf[c][(size_t) verbW] = v;
        }
        const float g = 1.55f / std::sqrt ((float) kTaps);
        float out[2] = { 0, 0 };
        lfoPh += 0.45f / sr; if (lfoPh >= 1.0f) lfoPh -= 1.0f;
        if (tapTick-- <= 0)
        {
            tapTick = 15;
            const float T = (0.030f + 0.97f * ms * 0.001f) * sr;
            for (int c = 0; c < 2; ++c)
                for (int k = 0; k < kTaps; ++k)
                {
                    const float wob = 0.00045f * sr * std::sin (kTwoPi * (lfoPh + tapLfoPh[c][k]));
                    tapDly[c][k] = std::max (1.0f, tapPos[c][k] * T + wob);
                    const float pos = tapPos[c][k];
                    tapGain[c][k] = (pos > 0.94f ? (1.0f - pos) / 0.06f : 1.0f) * (pos < 0.05f ? 0.55f + 9.0f * pos : 1.0f) * (k & 1 ? -1.0f : 1.0f);
                }
        }
        for (int c = 0; c < 2; ++c)
        {
            float s = 0.0f;
            for (int k = 0; k < kTaps; ++k)
            {
                float fpos = (float) verbW - tapDly[c][k];
                if (fpos < 0.0f) fpos += (float) size;
                int i0 = (int) fpos; const float fr = fpos - (float) i0;
                if (i0 >= size) i0 -= size;
                const int i1 = i0 + 1 < size ? i0 + 1 : 0;
                const float* vb = verbBuf[c].data();
                const float smp = vb[i0] + fr * (vb[i1] - vb[i0]);
                s += smp * tapGain[c][k];                                   // montee rapide, plein, coupure franche
            }
            out[c] = vLp[c].lp (vHp[c].hp (s * g));
        }
        verbW = (verbW + 1) % size;
        wl = out[0]; wr = out[1];
    }

    // ------------------------------------------------------------------ lissage
    struct Smooth
    {
        float v = 0, t = 0, c = 0.999f;
        void reset (float sr, float sec, float init) { v = t = init; c = std::exp (-1.0f / (sec * sr)); }
        void target (float x) { t = x; }
        float next() { v = t + (v - t) * c; if (std::abs (v - t) < 1.0e-6f) v = t; return v; }   // atteint exactement la cible (bypass exact)
    };
    struct OnFade { float v = 0; void set (bool on) { want = on ? 1.0f : 0.0f; } float next() { v += (want - v) * 0.004f; if (std::abs (v - want) < 1e-4f) v = want; return v; } float want = 0; };

    static double rand01() { static std::uint32_t s = 12345u; s = s * 1664525u + 1013904223u; return (double) (s >> 8) / 16777216.0; }    // ------------------------------------------------------------------ etat
    float sr = 48000.0f;
    PitchTracker tracker;
    std::array<Voice, 3> voices;
    Svf inHp, onHp;
    float hfFast = 0, hfSlow = 0;
    float envFast = 0, envSlow = 0, amp = 0, fEnv = 0, logF = 7, logFTarget = 7, candLog = 0;
    bool haveNote = false;
    int holdN = 0, waitN = 0, onsetN = 0, candVotes = 0;

    Smooth sMix, sSynMix, sIn, sOut, sDist, sModD, sModM, sRingHz, sRingM, sCrHz, sBits, sVerbMs, sVerbM, sActive;
    std::array<Smooth, 2> modOnG;
    std::array<Smooth, 3> voiceGain;
    OnFade onG[4];
    DcBlock dcPre[4]; OnePole lpPre[4];
    float crushPh = 0, crushHold[2] {}, ringPh = 0;
    std::vector<float> verbBuf[2];
    Allpass ap[2][4];
    static constexpr float apMs[2][4] = { { 4.7f, 7.3f, 11.9f, 17.3f }, { 5.3f, 8.9f, 13.1f, 19.7f } };
    float tapPos[2][kTaps] {}, tapLfoPh[2][kTaps] {}, lfoPh = 0;
    float tapDly[2][kTaps] {}, tapGain[2][kTaps] {}; int tapTick = 0;
    void clearVerb()
    {
        for (auto& b : verbBuf) std::fill (b.begin(), b.end(), 0.0f);
        for (auto& c : ap) for (auto& a : c) std::fill (a.b.begin(), a.b.end(), 0.0f);
        vHp[0].reset(); vHp[1].reset(); vLp[0].reset(); vLp[1].reset();
    }
    Svf vHp[2], vLp[2];
    int verbW = 0;
};

} // namespace synthetika
