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

// ------------------------------------------------------------------ suivi de hauteur YIN monophonique
// Signal decime vers ~11 kHz, fenetre de 256 echantillons + decalage jusqu'a 300 (40 Hz : 7 cordes en G et plus bas),
// analyse toutes les 24 echantillons decimes (~2 ms). Mediane des 3 dernieres mesures contre les sauts d'octave.
class PitchTracker
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        dec = std::max (1, (int) std::lround (sampleRate / 11025.0));
        dsr = sr / (float) dec;
        aa1.set (dsr * 0.42f, 0.54f, sr); aa2.set (dsr * 0.42f, 1.31f, sr);
        hp.set (40.0f, 0.7f, sr);
        buf.assign (kBuf, 0.0f); w = 0; cnt = 0; hopCnt = 0;
        fbuf.assign (kFBuf, 0.0f); fw = 0;
        freq = 0; conf = 0; hist[0] = hist[1] = hist[2] = 0;
        d.assign (kTauMax + 2, 0.0f);
    }
    // entree mono au rythme de l'hote ; renvoie vrai si une nouvelle mesure est disponible
    bool push (float x)
    {
        const float xh = hp.hp (x);
        fbuf[(size_t) fw] = xh; fw = (fw + 1) & (kFBuf - 1);
        x = aa2.lp (aa1.lp (xh));
        if (++cnt < dec) return false;
        cnt = 0;
        buf[(size_t) w] = x; w = (w + 1) & (kBuf - 1);
        if (++hopCnt < kHop) return false;
        hopCnt = 0;
        analyse();
        return true;
    }
    float frequency() const { return freq; }
    float confidence() const { return conf; }
    int latencySamples() const { return (kWin + kTauMax) * dec / 2; }

private:
    static constexpr int kBuf = 1024, kWin = 256, kTauMax = 300, kHop = 24, kFBuf = 8192;     // 40 Hz : 7 cordes en G (49 Hz) et plus bas
    void analyse()
    {
        const int tauMin = std::max (2, (int) (dsr / 1400.0f));
        const int start = (w - kWin - kTauMax + kBuf) & (kBuf - 1);
        float frame[kWin + kTauMax];
        float energy = 0.0f;
        for (int i = 0; i < kWin + kTauMax; ++i) { frame[i] = buf[(size_t) ((start + i) & (kBuf - 1))]; }
        for (int i = kTauMax; i < kWin + kTauMax; ++i) energy += frame[i] * frame[i];
        if (energy < 1.0e-7f * kWin) { conf = 0.0f; return; }
        // difference : x(t) recent compare a x(t - tau)
        const float* now = frame + kTauMax;
        d[0] = 0.0f;
        float run = 0.0f, best = 2.0f; int bestTau = -1;
        for (int tau = 1; tau <= kTauMax; ++tau)
        {
            const float* past = now - tau;
            float s = 0.0f;
            for (int j = 0; j < kWin; ++j) { const float e = now[j] - past[j]; s += e * e; }
            run += s;
            d[(size_t) tau] = run > 0.0f ? s * (float) tau / run : 1.0f;     // difference cumulee normalisee
        }
        for (int tau = tauMin; tau < kTauMax; ++tau)
        {
            if (d[(size_t) tau] < 0.14f)
            {
                while (tau + 1 < kTauMax && d[(size_t) tau + 1] < d[(size_t) tau]) ++tau;
                bestTau = tau; best = d[(size_t) tau]; break;
            }
            if (d[(size_t) tau] < best) { best = d[(size_t) tau]; bestTau = tau; }
        }
        if (bestTau < tauMin || best > 0.45f) { conf = std::max (0.0f, 1.0f - best); conf *= 0.5f; return; }
        float t = (float) bestTau;
        if (bestTau > 1 && bestTau < kTauMax)
        {
            const float a = d[(size_t) bestTau - 1], b = d[(size_t) bestTau], c = d[(size_t) bestTau + 1];
            const float den = a - 2 * b + c;
            if (std::abs (den) > 1e-9f) t += 0.5f * (a - c) / den;
        }
        const float f = refine (t * (float) dec);
        hist[0] = hist[1]; hist[1] = hist[2]; hist[2] = f;
        float m = hist[2];
        if (hist[0] > 0 && hist[1] > 0)
        {
            float a = hist[0], b = hist[1], c = hist[2];
            m = std::max (std::min (a, b), std::min (std::max (a, b), c));
            // le dernier releve gagne s'il est proche de la mediane (suivi rapide des bends et vibratos)
            if (std::abs (std::log2 (c / m)) < 0.08f) m = c;
        }
        freq = m; conf = 1.0f - best;
    }

    // affinage a pleine resolution : difference normalisee autour de la periode grossiere, minimum parabolique
    float refine (float lag0)
    {
        const int L0 = (int) std::lround (lag0), span = dec * 2 + 1;
        const int Lmax = L0 + span;
        const int win = std::clamp (2 * L0, 256, 2048);
        if (Lmax + win + 2 >= kFBuf) return sr / lag0;
        float dv[64]; const int n = std::min (63, 2 * span + 1);
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
        const float L = (float) (L0 - span + bestI) + (std::abs (den) > 1e-12f ? 0.5f * (a - c) / den : 0.0f);
        return sr / L;
    }
    std::vector<float> fbuf; int fw = 0;
    float sr = 48000, dsr = 12000;
    int dec = 4, cnt = 0, hopCnt = 0, w = 0;
    Svf aa1, aa2, hp;
    std::vector<float> buf, d;
    float freq = 0, conf = 0, hist[3] {};
};

// ------------------------------------------------------------------ parametres
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
        for (auto& v : voices) for (auto& o : v.osc) o.ph = (float) (rand01() * 0.999);
        for (auto& v : voices) { v.f1.reset(); v.f2.reset(); v.lad.reset(); }
        envFast.reset(); amp = 0; logF = std::log2 (110.0f); logFTarget = logF; haveNote = false; gate = 0;
        inHp.set (25.0f, 0.7f, sr);
        // reverb a porte
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
        for (auto& d : dcPre) d.reset();
        for (auto& d : dcPost) d.reset();
        for (auto& l : lpPre) l.reset();
        for (auto& l : lpPost) l.reset();
        sMix.reset (sr, 0.03f, 1.0f); sSynMix.reset (sr, 0.03f, 1.0f); sIn.reset (sr, 0.03f, 1.0f); sOut.reset (sr, 0.03f, 1.0f);
        sDist.reset (sr, 0.03f, 0.4f); sModD.reset (sr, 0.05f, 0.3f); sModM.reset (sr, 0.03f, 0.5f);
        sRingHz.reset (sr, 0.03f, 440.f); sRingM.reset (sr, 0.03f, 0.5f); sCrHz.reset (sr, 0.03f, 44100.f); sBits.reset (sr, 0.03f, 24.f);
        sVerbMs.reset (sr, 0.08f, 300.f); sVerbM.reset (sr, 0.03f, 0.3f); sActive.reset (sr, 0.015f, 1.0f);
        for (auto& g : modOnG) g.reset (sr, 0.01f, 0.0f);
        voiceGain[0].reset (sr, 0.02f, 1.0f); voiceGain[1].reset (sr, 0.02f, 1.0f); voiceGain[2].reset (sr, 0.02f, 1.0f);
        inPeak = outPeak = 0.0f;
        (void) maxBlock;
    }

    float inPeak = 0.0f, outPeak = 0.0f;       // pour les LED (lus par l'interface)
    float trackedHz() const { return haveNote ? std::exp2 (logF) : 0.0f; }

    // io : 1 ou 2 canaux, traitement en place
    void process (float* const* io, int numCh, int n, const Params& p)
    {
        sMix.target (p.mix); sSynMix.target (p.synthMix); sIn.target (p.inGain); sOut.target (p.outGain);
        sDist.target (p.distGain); sModD.target (p.modDepth); sModM.target (p.modMix);
        sRingHz.target (p.ringHz); sRingM.target (p.ringMix); sCrHz.target (std::min (p.crushHz, 44100.0f)); sBits.target (p.crushBits);
        sVerbMs.target (p.verbMs); sVerbM.target (p.verbMix); sActive.target (p.active ? 1.0f : 0.0f);
        modOnG[0].target (p.modOn ? 1.0f : 0.0f);
        modOnG[1].target (p.synthOn ? 1.0f : 0.0f);
        // modules : fondu court a l'activation (pas de clic)
        onG[0].set (p.distOn); onG[1].set (p.ringOn); onG[2].set (p.crushOn); onG[3].set (p.verbOn);
        voiceGain[0].target (1.0f);
        voiceGain[1].target (p.engine == 4 ? 1.0f : 0.0f);
        voiceGain[2].target (p.engine == 4 && p.int2On ? 1.0f : 0.0f);
        int ord[3]; orderOf (p.order, ord);
        for (auto& v : voices) v.engine = std::clamp (p.engine, 0, 4);
        const EngineSpec& es = spec (p.engine);
        const float atkC = std::exp (-1.0f / (es.attack * sr)), relC = std::exp (-1.0f / (es.release * sr));

        float* L = io[0]; float* R = numCh > 1 ? io[1] : nullptr;
        for (int i = 0; i < n; ++i)
        {
            const float inRaw = R ? 0.5f * (L[i] + R[i]) : L[i];
            const float active = sActive.next();
            const float gIn = sIn.next();
            const float x = inRaw * gIn;
            inPeak = std::max (inPeak * 0.9995f, std::abs (x));

            // ---- suivi de hauteur et enveloppe de la corde
            if (tracker.push (x))
            {
                const float f = tracker.frequency(), c = tracker.confidence();
                if (f > 40.0f && f < 1500.0f && c > 0.55f && envFast.z > 0.0015f)
                {
                    logFTarget = std::log2 (f);
                    if (! haveNote) { logF = logFTarget; haveNote = true; }
                    voicedHold = (int) (0.06f * sr);
                }
            }
            const float ax = std::abs (inHp.hp (x));
            envFast.z += (ax > envFast.z ? 0.35f : 0.0009f) * (ax - envFast.z);      // ~0,1 ms montee / ~25 ms descente
            if (voicedHold > 0) --voicedHold;
            const float target = (haveNote && (voicedHold > 0 || envFast.z > 0.004f)) ? envFast.z * 1.55f : 0.0f;
            amp = target > amp ? target + (amp - target) * atkC : target + (amp - target) * relC;
            // glissement continu sur l'echelle log (suit bends, slides et vibrato, aucune quantification)
            logF += (logFTarget - logF) * glideCoef;

            // ---- synthe
            float synL = 0.0f, synR = 0.0f;
            const float synOn = modOnG[1].next();
            if (synOn > 0.0001f && haveNote)
            {
                const float depth = sModD.next() * modOnG[0].next();
                const float cents = depth * 32.0f;
                const float det = std::exp2 (cents / 1200.0f);
                const float mm = sModM.next() * (depth > 0.0001f ? 1.0f : 0.0f);
                const float semis[3] = { 0.0f, (float) p.int1, (float) p.int2 };
                float c = 0, fl = 0, sh = 0;
                for (int v = 0; v < 3; ++v)
                {
                    const float vg = voiceGain[v].next();
                    if (vg < 0.0001f) continue;
                    const float f = std::exp2 (logF + semis[v] / 12.0f);
                    float cc, ff, ss;
                    voices[(size_t) v].render (es, f, det, mm > 0.0001f, sr, amp, cc, ff, ss);
                    c += vg * cc; fl += vg * ff; sh += vg * ss;
                }
                const float norm = p.engine == 4 ? (p.int2On ? 0.62f : 0.75f) : 1.0f;
                // deux voix desaccordees (une plus basse a gauche, une plus haute a droite) melangees a la voix principale
                synL = norm * (c * (1.0f - 0.45f * mm) + fl * mm * 0.85f) * amp;
                synR = norm * (c * (1.0f - 0.45f * mm) + sh * mm * 0.85f) * amp;
                synL *= synOn; synR *= synOn;
            }
            else { modOnG[0].next(); sModD.next(); sModM.next(); for (auto& vg : voiceGain) vg.next(); }

            const float sm = sSynMix.next() * synOn;
            float l = x * (1.0f - sm) + synL * sm;
            float r = x * (1.0f - sm) + synR * sm;

            // ---- distorsion PRE
            const float dg = sDist.next();
            const float dOn = onG[0].next();
            if (dOn > 0.0f && p.distPre) { const float yl = shape (l, dg, p.distMode, 0), yr = shape (r, dg, p.distMode, 1); l += (yl - l) * dOn; r += (yr - r) * dOn; }

            // ---- bitcrush / ring mod / gated reverb dans l'ordre choisi
            const float crHz = sCrHz.next(), bits = sBits.next(), rHz = sRingHz.next(), rM = sRingM.next(), vMs = sVerbMs.next(), vM = sVerbM.next();
            const float gRing = onG[1].next(), gCr = onG[2].next(), gVerb = onG[3].next();
            for (int k = 0; k < 3; ++k)
            {
                switch (ord[k])
                {
                    case 0: if (gCr > 0.0f)   { float a = l, b = r; crush (a, b, crHz, bits); l += (a - l) * gCr; r += (b - r) * gCr; } break;
                    case 1: if (gRing > 0.0f) { const float car = std::sin (ringPh * kTwoPi); const float m = rM * gRing; l = l * (1 - m) + l * car * m; r = r * (1 - m) + r * car * m; } break;
                    case 2: if (gVerb > 0.0f) { float wl, wr; verb (l, r, vMs, wl, wr); const float m = vM * gVerb; l = l * (1.0f - 0.5f * m) + wl * m; r = r * (1.0f - 0.5f * m) + wr * m; }
                            else { verbBuf[0][(size_t) verbW] = 0.0f; verbBuf[1][(size_t) verbW] = 0.0f; verbW = (verbW + 1) % (int) verbBuf[0].size(); } break;
                    default: break;
                }
            }
            ringPh += rHz / sr; if (ringPh >= 1.0f) ringPh -= 1.0f;

            // ---- distorsion POST
            if (dOn > 0.0f && p.distPost) { const float yl = shape (l, dg, p.distMode, 2), yr = shape (r, dg, p.distMode, 3); l += (yl - l) * dOn; r += (yr - r) * dOn; }

            // ---- mix global et sortie
            const float m = sMix.next(), go = sOut.next();
            float ol = (x * (1.0f - m) + l * m) * go;
            float orr = (x * (1.0f - m) + r * m) * go;
            // footswitch : fondu avec le signal d'entree d'origine
            ol = inRaw + (ol - inRaw) * active; orr = inRaw + (orr - inRaw) * active;
            if (! std::isfinite (ol)) ol = 0; if (! std::isfinite (orr)) orr = 0;
            outPeak = std::max (outPeak * 0.9995f, std::max (std::abs (ol), std::abs (orr)));
            if (R) { L[i] = ol; R[i] = orr; } else L[i] = 0.5f * (ol + orr);
        }
    }

private:
    // ------------------------------------------------------------------ moteurs (timbre et enveloppe)
    struct EngineSpec { float attack, release; };
    static const EngineSpec& spec (int e)
    {
        static const EngineSpec s[5] = {
            { 0.070f, 0.045f },   // DEEP : attaque ~80 ms mesuree sur l'extrait
            { 0.018f, 0.080f },   // VINTAGE
            { 0.005f, 0.050f },   // SOLO
            { 0.200f, 0.380f },   // STRINGS : nappe lente
            { 0.022f, 0.060f },   // HARMONY
        };
        return s[std::clamp (e, 0, 4)];
    }

    struct Voice
    {
        std::array<Osc, 12> osc;    // 4 oscillateurs x (centre, bas, haut)
        Svf f1, f2, f1b, f2b, f1c, f2c; Ladder lad, ladB, ladC;
        int lastEngine = -1;
        // rend la voix centrale et ses copies desaccordees (bas = gauche, haut = droite)
        void render (const EngineSpec&, float f, float det, bool withCopies, float sr, float env, float& c, float& lo, float& hi)
        {
            c = one (0, f, sr, env, f1, lad);
            if (withCopies) { lo = one (4, f / det, sr, env, f1b, ladB); hi = one (8, f * det, sr, env, f1c, ladC); }
            else { lo = hi = c; }
        }
        int engine = 0;
        float one (int base, float f, float sr, float env, Svf& filt, Ladder& ld)
        {
            const float dt = std::min (0.45f, f / sr);
            float s0, q0, s1, q1, s2, q2, s3, q3;
            switch (engine)
            {
                default:
                case 0:   // DEEP : carre a 48 % (quelques pairs) + sinus de renfort sur la fondamentale, passe-bas doux
                {
                    osc[(size_t) base].tick (dt, 0.48f, s0, q0);
                    const float sine = std::sin (osc[(size_t) base].ph * kTwoPi);
                    filt.set (std::min (3200.0f, f * 28.0f), 0.62f, sr);
                    return filt.lp (0.80f * q0 + 0.36f * sine) * 0.43f;
                }
                case 1:   // VINTAGE : deux scies a +/-5 cents dans un passe-bas 4 poles qui s'ouvre avec l'attaque
                {
                    osc[(size_t) base].tick (dt * 1.0029f, 0.5f, s0, q0);
                    osc[(size_t) base + 1].tick (dt * 0.9971f, 0.5f, s1, q1);
                    const float cut = f * (2.2f + 26.0f * std::min (1.0f, env * 4.0f));
                    ld.set (std::min (cut, 9000.0f), 0.22f, sr);
                    return ld.run (0.62f * s0 + 0.40f * s1) * 1.5f;     // voix inegales : jamais d'annulation complete du fondamental
                }
                case 2:   // SOLO : scie + impulsion 25 %, passe-bas resonant, legere saturation (lead analogique)
                {
                    osc[(size_t) base].tick (dt, 0.5f, s0, q0);
                    osc[(size_t) base + 1].tick (dt, 0.25f, s1, q1);
                    filt.set (std::min (5200.0f, f * 12.0f), 1.25f, sr);
                    return std::tanh (1.4f * filt.lp (0.6f * s0 - 0.45f * q1)) * 0.68f;   // impulsion en opposition : fondamental renforce
                }
                case 3:   // STRINGS : trois scies desaccordees (-11 / 0 / +11 cents) + octave douce, filtre sombre
                {
                    osc[(size_t) base].tick (dt * 0.99366f, 0.5f, s0, q0);
                    osc[(size_t) base + 1].tick (dt, 0.5f, s1, q1);
                    osc[(size_t) base + 2].tick (dt * 1.00637f, 0.5f, s2, q2);
                    osc[(size_t) base + 3].tick (std::min (0.45f, dt * 2.0f), 0.5f, s3, q3);
                    filt.set (std::min (3200.0f, f * 9.0f), 0.55f, sr);
                    return filt.lp (0.36f * (s0 + s1 + s2) + 0.14f * s3) * 1.15f;
                }
                case 4:   // HARMONY : carre + scie, passe-bas moyen (chaque intervalle est une voix de ce type)
                {
                    osc[(size_t) base].tick (dt, 0.5f, s0, q0);
                    filt.set (std::min (3600.0f, f * 14.0f), 0.7f, sr);
                    return filt.lp (0.55f * q0 + 0.45f * s0) * 1.1f;
                }
            }
        }
    };

    // ------------------------------------------------------------------ distorsion (meme gain et mode avant / apres)
    float shape (float x, float g01, int mode, int st)
    {
        const float driveDb = 36.0f * g01;
        const float d = dbToGain (driveDb);
        const float comp = 1.0f / (1.0f + 0.055f * driveDb);
        float y;
        switch (mode)
        {
            default:
            case 0: y = std::tanh (d * x * 1.2f); break;                                                      // SOFT : arrondi, facon lampe
            case 1: { const float v = d * x * 2.0f + 0.08f; y = v / std::pow (1.0f + std::pow (std::abs (v), 6.0f), 1.0f / 6.0f) - 0.08f; break; } // HARD : ecretage franc
            case 2: { const float v = d * x * 1.3f; y = std::tanh (v + 0.32f * v * v) ; break; }               // RICH : harmoniques paires
            case 3: { const float v = d * x * 1.6f; y = std::sin (v * 1.5708f); break; }                       // WAVEFOLD : repli sur soi
        }
        y = dcPre[st].run (y);
        lpPre[st].setHz (11000.0f, sr);
        return lpPre[st].lp (y) * comp;
    }

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
        const float T = (0.030f + 0.97f * ms * 0.001f) * sr;
        const float g = 1.55f / std::sqrt ((float) kTaps);
        float out[2] = { 0, 0 };
        lfoPh += 0.45f / sr; if (lfoPh >= 1.0f) lfoPh -= 1.0f;
        for (int c = 0; c < 2; ++c)
        {
            float s = 0.0f;
            for (int k = 0; k < kTaps; ++k)
            {
                const float pos = tapPos[c][k];
                const float wob = 0.00045f * sr * std::sin (kTwoPi * (lfoPh + tapLfoPh[c][k]));
                const float dly = std::max (1.0f, pos * T + wob);
                const float fpos = (float) verbW - dly;
                int i0 = (int) std::floor (fpos); const float fr = fpos - (float) i0;
                i0 = ((i0 % size) + size) % size; const int i1 = (i0 + 1) % size;
                const float smp = verbBuf[c][(size_t) i0] + fr * (verbBuf[c][(size_t) i1] - verbBuf[c][(size_t) i0]);
                const float tail = pos > 0.94f ? (1.0f - pos) / 0.06f : 1.0f;            // coupure franche
                const float build = pos < 0.05f ? 0.55f + 9.0f * pos : 1.0f;            // montee rapide des reflexions
                s += smp * tail * build * (k & 1 ? -1.0f : 1.0f);
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
        float next() { v = t + (v - t) * c; return v; }
    };
    struct OnFade { float v = 0; void set (bool on) { want = on ? 1.0f : 0.0f; } float next() { v += (want - v) * 0.004f; if (std::abs (v - want) < 1e-4f) v = want; return v; } float want = 0; };

    static double rand01() { static std::uint32_t s = 12345u; s = s * 1664525u + 1013904223u; return (double) (s >> 8) / 16777216.0; }

    float sr = 48000;
    PitchTracker tracker;
    OnePole envFast;
    Svf inHp;
    float amp = 0, logF = 7, logFTarget = 7, glideCoef = 0.0035f;
    bool haveNote = false; int voicedHold = 0; float gate = 0;
    std::array<Voice, 3> voices;
    Smooth sMix, sSynMix, sIn, sOut, sDist, sModD, sModM, sRingHz, sRingM, sCrHz, sBits, sVerbMs, sVerbM, sActive;
    std::array<Smooth, 2> modOnG;
    std::array<Smooth, 3> voiceGain;
    std::array<OnFade, 4> onG;
    DcBlock dcPre[4]; OnePole lpPre[4]; DcBlock dcPost[4]; OnePole lpPost[4];
    float crushPh = 0, crushHold[2] {};
    float ringPh = 0;
    std::vector<float> verbBuf[2]; int verbW = 0;
    Allpass ap[2][4];
    static constexpr float apMs[2][4] = { { 4.7f, 7.3f, 11.9f, 17.3f }, { 5.3f, 8.9f, 13.1f, 19.7f } };
    float tapPos[2][kTaps] {}, tapLfoPh[2][kTaps] {}, lfoPh = 0;
    Svf vHp[2], vLp[2];

public:
    void setEngine (int e) { for (auto& v : voices) v.engine = std::clamp (e, 0, 4); }
};
} // namespace synthetika
