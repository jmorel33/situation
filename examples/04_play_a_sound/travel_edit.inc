/* Unified parameter travel — included from main.c */

#define TRV_SEC_ADSR   0
#define TRV_SEC_OSC    1
#define TRV_SEC_FILTER 2
#define TRV_SEC_SUB    3
#define TRV_SEC_LFO    4
#define TRV_SEC_FX     5
#define TRV_SEC_COUNT  6

static const char* k_trv_section_titles[TRV_SEC_COUNT] = {
    "ADSR", "OSCILLATOR", "FILTER", "SUB-OSC", "LFO", "EFFECTS"
};

typedef enum {
    TRV_KIND_FLOAT, TRV_KIND_ENUM, TRV_KIND_BOOL, TRV_KIND_ACTION, TRV_KIND_LOG
} TrvKind;

typedef enum {
    TRV_F_ATTACK, TRV_F_DECAY, TRV_F_SUSTAIN, TRV_F_RELEASE, TRV_F_HOLD,
    TRV_F_FREQUENCY, TRV_F_VOLUME,
    TRV_F_WAVEFORM, TRV_F_PWM, TRV_F_PAN, TRV_F_VOICE_MODE, TRV_F_GLIDE, TRV_F_GLIDE_T,
    TRV_F_GLIDE_SPEED,
    TRV_F_GLIDE_RESET,
    TRV_F_PATCH_SLOT, TRV_F_PATCH_STORE,
    TRV_F_FILTER_MODE, TRV_F_FILTER_POLES, TRV_F_FILTER_CUTOFF, TRV_F_FILTER_Q,
    TRV_F_FILTER_DRIVE, TRV_F_FILTER_KEYTRACK, TRV_F_FILTER_OS,
    TRV_F_FILTER_ENV, TRV_F_FILTER_ENV_RNG,
    TRV_F_SUB_LEVEL, TRV_F_SUB_COARSE, TRV_F_SUB_FINE, TRV_F_SUB_OCTAVE, TRV_F_SUB_WAVE,
    TRV_F_SUB_SYNC, TRV_F_SUB_RING,
    TRV_F_LFO_RATE, TRV_F_LFO_WAVE, TRV_F_LFO_PITCH_AMT, TRV_F_LFO_PITCH_RANGE,
    TRV_F_LFO_PWM_AMT, TRV_F_LFO_PWM_RANGE, TRV_F_LFO_FILT_AMT, TRV_F_LFO_FILT_RANGE,
    TRV_F_LFO_RESET, TRV_F_LFO_PHASE
} TrvField;

typedef struct {
    TrvField    field;
    const char* label;
    TrvKind     kind;
    float       min_val;
    float       max_val;
    float       step;
    float       fine_step;
    int         enum_max;
} TrvDef;

typedef struct {
    const TrvDef* params;
    int           count;
} TrvSectionDef;

static const TrvDef k_trv_adsr[] = {
    { TRV_F_ATTACK,  "Attack",  TRV_KIND_FLOAT, 0.001f, 2.0f,   0.005f, 0.001f, 0 },
    { TRV_F_DECAY,   "Decay",   TRV_KIND_FLOAT, 0.001f, 2.0f,   0.005f, 0.001f, 0 },
    { TRV_F_SUSTAIN, "Sustain", TRV_KIND_FLOAT, 0.0f,   1.0f,   0.05f,  0.01f,  0 },
    { TRV_F_RELEASE, "Release", TRV_KIND_FLOAT, 0.01f,  5.0f,   0.05f,  0.01f,  0 },
    { TRV_F_HOLD,    "Hold",    TRV_KIND_FLOAT, -1.0f, 10.0f,  0.25f,  0.05f,  0 },
};

static const TrvDef k_trv_osc[] = {
    { TRV_F_FREQUENCY,  "Freq",    TRV_KIND_LOG,   20.f,  20000.f, 0.0f,  0.0f,  0 },
    { TRV_F_VOLUME,     "Level",   TRV_KIND_FLOAT, 0.0f,  1.0f,    0.05f, 0.01f, 0 },
    { TRV_F_WAVEFORM,   "Wave",    TRV_KIND_ENUM,  0.0f, 4.0f,  1.0f,  1.0f,  4 },
    { TRV_F_PWM,        "Pulse",   TRV_KIND_FLOAT, 0.05f, 0.95f, 0.02f, 0.005f, 0 },
    { TRV_F_PAN,        "Pan",     TRV_KIND_FLOAT, -1.0f, 1.0f,  0.05f, 0.01f, 0 },
    { TRV_F_VOICE_MODE, "Mode",    TRV_KIND_ENUM,  0.0f, 1.0f,  1.0f,  1.0f,  1 },
    { TRV_F_GLIDE,      "Glide",   TRV_KIND_BOOL,  0.0f, 1.0f,  1.0f,  1.0f,  0 },
    { TRV_F_GLIDE_T,    "Glide t", TRV_KIND_FLOAT, 0.01f, 2.0f,  0.02f, 0.005f, 0 },
    { TRV_F_GLIDE_SPEED,"Gl spd",  TRV_KIND_FLOAT, 0.0f,  48.0f, 1.0f,  0.25f, 0 },
    { TRV_F_GLIDE_RESET,"Env key", TRV_KIND_BOOL,  0.0f,  1.0f,  1.0f,  1.0f,  0 },
    { TRV_F_PATCH_SLOT, "Slot",    TRV_KIND_FLOAT, 0.0f, 15.0f, 1.0f,  1.0f,  0 },
    { TRV_F_PATCH_STORE,"Save",    TRV_KIND_ACTION,0.0f, 0.0f,  0.0f,  0.0f,  0 },
};

static const TrvDef k_trv_filter[] = {
    { TRV_F_FILTER_MODE,   "Mode",   TRV_KIND_ENUM, 0.0f, 8.0f,    1.0f, 1.0f, 8 },
    { TRV_F_FILTER_POLES,  "Slope",  TRV_KIND_ENUM, 1.0f, 4.0f,    1.0f, 1.0f, 3 },
    { TRV_F_FILTER_CUTOFF, "Cutoff", TRV_KIND_LOG,  20.f, 20000.f, 0.0f, 0.0f, 0 },
    { TRV_F_FILTER_Q,      "Q",      TRV_KIND_FLOAT,0.5f, 20.f,    0.5f, 0.1f, 0 },
    { TRV_F_FILTER_DRIVE,  "Drive",  TRV_KIND_FLOAT,1.0f, 10.0f,  0.25f,0.05f,0 },
    { TRV_F_FILTER_KEYTRACK,"Keytrk", TRV_KIND_FLOAT,0.0f, 1.0f,    0.05f,0.01f,0 },
    { TRV_F_FILTER_OS,     "OS 2x",  TRV_KIND_ENUM, 0.0f, 1.0f,    1.0f, 1.0f, 1 },
    { TRV_F_FILTER_ENV,    "Env",    TRV_KIND_FLOAT,0.0f, 1.0f,    0.05f,0.01f,0 },
    { TRV_F_FILTER_ENV_RNG,"Env rng",TRV_KIND_LOG,  20.f, 8000.f,  0.0f, 0.0f, 0 },
};

static const char* k_filter_mode_labels[] = {
    "Off", "LP", "HP", "BP", "Notch", "AP", "LP+BP", "LP+HP", "BP+HP"
};
static const char* k_filter_os_labels[] = { "Off", "2x" };
static const char* k_slope_labels[] = { "6dB", "12dB", "18dB", "24dB" };
static const char* k_lfo_wave_labels[] = { "Tri", "Square", "Random" };
static const char* k_sub_oct_labels[] = { "Unison", "Oct-1", "Oct-2" };

static const TrvDef k_trv_sub[] = {
    { TRV_F_SUB_LEVEL,  "Level",  TRV_KIND_FLOAT, 0.0f,  1.0f,  0.05f, 0.01f, 0 },
    { TRV_F_SUB_COARSE, "Coarse", TRV_KIND_FLOAT, -12.f, 12.0f, 1.0f,  0.25f, 0 },
    { TRV_F_SUB_FINE,   "Fine",   TRV_KIND_FLOAT, -1.0f, 1.0f,  0.05f, 0.01f, 0 },
    { TRV_F_SUB_OCTAVE, "Octave", TRV_KIND_ENUM,  0.0f,  2.0f,  1.0f,  1.0f,  2 },
    { TRV_F_SUB_WAVE,   "Wave",   TRV_KIND_ENUM,  0.0f,  4.0f,  1.0f,  1.0f,  4 },
    { TRV_F_SUB_SYNC,   "Sync",   TRV_KIND_BOOL,  0.0f,  1.0f,  1.0f,  1.0f,  0 },
    { TRV_F_SUB_RING,   "Ring",   TRV_KIND_BOOL,  0.0f,  1.0f,  1.0f,  1.0f,  0 },
};

static const TrvDef k_trv_lfo[] = {
    { TRV_F_LFO_RATE,        "Rate",     TRV_KIND_FLOAT, 0.0f,  20.0f, 0.2f,  0.05f, 0 },
    { TRV_F_LFO_WAVE,        "Wave",     TRV_KIND_ENUM,  0.0f,  2.0f,  1.0f,  1.0f,  2 },
    { TRV_F_LFO_PITCH_AMT,   "Pit amt",  TRV_KIND_FLOAT, 0.0f,  1.0f,  0.05f, 0.01f, 0 },
    { TRV_F_LFO_PITCH_RANGE, "Pit rng",  TRV_KIND_FLOAT, 0.0f,  12.0f, 0.25f, 0.05f, 0 },
    { TRV_F_LFO_PWM_AMT,     "PWM amt",  TRV_KIND_FLOAT, 0.0f,  1.0f,  0.05f, 0.01f, 0 },
    { TRV_F_LFO_PWM_RANGE,   "PWM rng",  TRV_KIND_FLOAT, 0.0f,  0.45f, 0.02f, 0.005f,0 },
    { TRV_F_LFO_FILT_AMT,    "Flt amt",  TRV_KIND_FLOAT, 0.0f,  1.0f,  0.05f, 0.01f, 0 },
    { TRV_F_LFO_FILT_RANGE,  "Flt rng",  TRV_KIND_LOG,  20.f,  8000.f,0.0f,  0.0f,  0 },
    { TRV_F_LFO_RESET,       "Rst key",  TRV_KIND_BOOL,  0.0f,  1.0f,  1.0f,  1.0f,  0 },
    { TRV_F_LFO_PHASE,       "Phase",    TRV_KIND_FLOAT, 0.0f,  1.0f,  0.04f, 0.01f, 0 },
};

static const TrvSectionDef k_trv_sections[TRV_SEC_COUNT] = {
    { k_trv_adsr,   (int)(sizeof k_trv_adsr   / sizeof k_trv_adsr[0])   },
    { k_trv_osc,    (int)(sizeof k_trv_osc    / sizeof k_trv_osc[0])    },
    { k_trv_filter, (int)(sizeof k_trv_filter / sizeof k_trv_filter[0]) },
    { k_trv_sub,    (int)(sizeof k_trv_sub    / sizeof k_trv_sub[0])    },
    { k_trv_lfo,    (int)(sizeof k_trv_lfo    / sizeof k_trv_lfo[0])    },
    { NULL,         0 },
};

static int trv_section_param_count(int section)
{
    if (section < 0 || section >= TRV_SEC_COUNT) return 0;
    if (section == TRV_SEC_FX)
        return 1 + k_fx_groups[s_fx_group].param_count;
    return k_trv_sections[section].count;
}

static void trv_recall_patch_slot(SituationAudioGraph* graph, const Nodes* nodes, int slot)
{
    if (slot < 0) slot = 0;
    if (slot > 15) slot = 15;
    s_patch_slot = slot;
    SituationSetControl(graph, nodes->tone, TONE_CTRL_PATCH_SLOT, (float)slot);
    pull_tone_params(graph, nodes->tone);
}

static void trv_store_patch(SituationAudioGraph* graph, const Nodes* nodes)
{
    push_params(graph, nodes);
    SituationSetControl(graph, nodes->tone, TONE_CTRL_PATCH_STORE, 1.0f);
    SituationSetControl(graph, nodes->tone, TONE_CTRL_PATCH_STORE, 0.0f);
}

static bool trv_read_field(TrvField field, float* out)
{
    if (!out) return false;
    switch (field) {
        case TRV_F_ATTACK:          *out = s_attack; return true;
        case TRV_F_DECAY:           *out = s_decay; return true;
        case TRV_F_SUSTAIN:         *out = s_sustain; return true;
        case TRV_F_RELEASE:         *out = s_release; return true;
        case TRV_F_HOLD:            *out = s_hold; return true;
        case TRV_F_FREQUENCY:       *out = s_frequency; return true;
        case TRV_F_VOLUME:          *out = s_volume; return true;
        case TRV_F_WAVEFORM:        *out = (float)s_waveform; return true;
        case TRV_F_PWM:             *out = s_pwm; return true;
        case TRV_F_PAN:             *out = s_pan; return true;
        case TRV_F_VOICE_MODE:      *out = s_mono ? 1.0f : 0.0f; return true;
        case TRV_F_GLIDE:           *out = s_portamento ? 1.0f : 0.0f; return true;
        case TRV_F_GLIDE_T:         *out = s_portamento_time; return true;
        case TRV_F_GLIDE_SPEED:     *out = s_portamento_speed; return true;
        case TRV_F_GLIDE_RESET:      *out = s_glide_reset_on_key ? 1.0f : 0.0f; return true;
        case TRV_F_PATCH_SLOT:      *out = (float)s_patch_slot; return true;
        case TRV_F_PATCH_STORE:     *out = 0.0f; return true;
        case TRV_F_FILTER_MODE:     *out = (float)s_filt_mode; return true;
        case TRV_F_FILTER_POLES:    *out = (float)s_filt_poles; return true;
        case TRV_F_FILTER_CUTOFF:   *out = s_filt_cut; return true;
        case TRV_F_FILTER_Q:        *out = s_filt_res; return true;
        case TRV_F_FILTER_DRIVE:    *out = s_filt_drive; return true;
        case TRV_F_FILTER_KEYTRACK: *out = s_filt_keytrack; return true;
        case TRV_F_FILTER_OS:       *out = (float)s_filt_os; return true;
        case TRV_F_FILTER_ENV:      *out = s_filt_env; return true;
        case TRV_F_FILTER_ENV_RNG:  *out = s_filt_env_range; return true;
        case TRV_F_SUB_LEVEL:       *out = s_sub_level; return true;
        case TRV_F_SUB_COARSE:      *out = s_sub_coarse; return true;
        case TRV_F_SUB_FINE:        *out = s_sub_fine; return true;
        case TRV_F_SUB_OCTAVE:      *out = (float)s_sub_oct; return true;
        case TRV_F_SUB_WAVE:        *out = (float)s_sub_wave; return true;
        case TRV_F_SUB_SYNC:        *out = s_sub_sync ? 1.0f : 0.0f; return true;
        case TRV_F_SUB_RING:        *out = s_sub_ring ? 1.0f : 0.0f; return true;
        case TRV_F_LFO_RATE:        *out = s_lfo_rate; return true;
        case TRV_F_LFO_WAVE:        *out = (float)s_lfo_wave; return true;
        case TRV_F_LFO_PITCH_AMT:   *out = s_lfo_pitch_amt; return true;
        case TRV_F_LFO_PITCH_RANGE: *out = s_lfo_pitch_range; return true;
        case TRV_F_LFO_PWM_AMT:     *out = s_lfo_pwm_amt; return true;
        case TRV_F_LFO_PWM_RANGE:   *out = s_lfo_pwm_range; return true;
        case TRV_F_LFO_FILT_AMT:    *out = s_lfo_filt_amt; return true;
        case TRV_F_LFO_FILT_RANGE:  *out = s_lfo_filt_range; return true;
        case TRV_F_LFO_RESET:       *out = s_lfo_reset_on_key ? 1.0f : 0.0f; return true;
        case TRV_F_LFO_PHASE:       *out = s_lfo_start_phase; return true;
        default: return false;
    }
}

static void trv_write_field(SituationAudioGraph* graph, const Nodes* nodes,
                            TrvField field, float v)
{
    switch (field) {
        case TRV_F_ATTACK:          s_attack = clampf(v, 0.001f, 2.0f); break;
        case TRV_F_DECAY:           s_decay = clampf(v, 0.001f, 2.0f); break;
        case TRV_F_SUSTAIN:         s_sustain = clampf(v, 0.0f, 1.0f); break;
        case TRV_F_RELEASE:         s_release = clampf(v, 0.01f, 5.0f); break;
        case TRV_F_HOLD:            s_hold = clampf(v, -1.0f, 10.0f); break;
        case TRV_F_FREQUENCY:       s_frequency = clampf(v, 20.f, 20000.f); break;
        case TRV_F_VOLUME:          s_volume = clampf(v, 0.0f, 1.0f); break;
        case TRV_F_WAVEFORM:
            s_waveform = (int)(v + 0.5f);
            if (s_waveform < 0) s_waveform = 0;
            if (s_waveform > 4) s_waveform = 4;
            break;
        case TRV_F_PWM:             s_pwm = clampf(v, 0.05f, 0.95f); break;
        case TRV_F_PAN:             s_pan = clampf(v, -1.0f, 1.0f); break;
        case TRV_F_VOICE_MODE:      s_mono = (v >= 0.5f); break;
        case TRV_F_GLIDE:           s_portamento = (v >= 0.5f); break;
        case TRV_F_GLIDE_T:         s_portamento_time = clampf(v, 0.01f, 2.0f); break;
        case TRV_F_GLIDE_SPEED:     s_portamento_speed = clampf(v, 0.0f, 48.0f); break;
        case TRV_F_GLIDE_RESET:      s_glide_reset_on_key = (v >= 0.5f); break;
        case TRV_F_PATCH_SLOT:
            trv_recall_patch_slot(graph, nodes, (int)(v + 0.5f));
            return;
        case TRV_F_PATCH_STORE:
            return;
        case TRV_F_FILTER_MODE:
            s_filt_mode = (int)(v + 0.5f);
            if (s_filt_mode < 0) s_filt_mode = 0;
            if (s_filt_mode > 8) s_filt_mode = 8;
            break;
        case TRV_F_FILTER_POLES:
            s_filt_poles = (int)(v + 0.5f);
            if (s_filt_poles < 1) s_filt_poles = 1;
            if (s_filt_poles > 4) s_filt_poles = 4;
            break;
        case TRV_F_FILTER_CUTOFF:   s_filt_cut = clampf(v, 20.f, 20000.f); break;
        case TRV_F_FILTER_Q:        s_filt_res = clampf(v, 0.5f, 20.f); break;
        case TRV_F_FILTER_DRIVE:    s_filt_drive = clampf(v, 1.0f, 10.0f); break;
        case TRV_F_FILTER_KEYTRACK: s_filt_keytrack = clampf(v, 0.0f, 1.0f); break;
        case TRV_F_FILTER_OS:
            s_filt_os = (int)(v + 0.5f);
            if (s_filt_os < 0) s_filt_os = 0;
            if (s_filt_os > 1) s_filt_os = 1;
            break;
        case TRV_F_FILTER_ENV:      s_filt_env = clampf(v, 0.0f, 1.0f); break;
        case TRV_F_FILTER_ENV_RNG:  s_filt_env_range = clampf(v, 20.f, 8000.f); break;
        case TRV_F_SUB_LEVEL:       s_sub_level = clampf(v, 0.0f, 1.0f); break;
        case TRV_F_SUB_COARSE:      s_sub_coarse = clampf(v, -12.f, 12.f); break;
        case TRV_F_SUB_FINE:        s_sub_fine = clampf(v, -1.0f, 1.0f); break;
        case TRV_F_SUB_OCTAVE:
            s_sub_oct = (int)(v + 0.5f);
            if (s_sub_oct < 0) s_sub_oct = 0;
            if (s_sub_oct > 2) s_sub_oct = 2;
            break;
        case TRV_F_SUB_WAVE:
            s_sub_wave = (int)(v + 0.5f);
            if (s_sub_wave < 0) s_sub_wave = 0;
            if (s_sub_wave > 4) s_sub_wave = 4;
            break;
        case TRV_F_SUB_SYNC:        s_sub_sync = (v >= 0.5f); break;
        case TRV_F_SUB_RING:        s_sub_ring = (v >= 0.5f); break;
        case TRV_F_LFO_RATE:        s_lfo_rate = clampf(v, 0.0f, 20.0f); break;
        case TRV_F_LFO_WAVE:
            s_lfo_wave = (int)(v + 0.5f);
            if (s_lfo_wave < 0) s_lfo_wave = 0;
            if (s_lfo_wave > 2) s_lfo_wave = 2;
            break;
        case TRV_F_LFO_PITCH_AMT:   s_lfo_pitch_amt = clampf(v, 0.0f, 1.0f); break;
        case TRV_F_LFO_PITCH_RANGE: s_lfo_pitch_range = clampf(v, 0.0f, 12.0f); break;
        case TRV_F_LFO_PWM_AMT:     s_lfo_pwm_amt = clampf(v, 0.0f, 1.0f); break;
        case TRV_F_LFO_PWM_RANGE:   s_lfo_pwm_range = clampf(v, 0.0f, 0.45f); break;
        case TRV_F_LFO_FILT_AMT:    s_lfo_filt_amt = clampf(v, 0.0f, 1.0f); break;
        case TRV_F_LFO_FILT_RANGE:  s_lfo_filt_range = clampf(v, 20.f, 8000.f); break;
        case TRV_F_LFO_RESET:       s_lfo_reset_on_key = (v >= 0.5f); break;
        case TRV_F_LFO_PHASE:       s_lfo_start_phase = clampf(v, 0.0f, 1.0f); break;
        default:
            return;
    }
    push_params(graph, nodes);
}

static bool trv_read(int section, int param, SituationAudioGraph* graph,
                     const Nodes* nodes, float* out)
{
    if (!out) return false;
    if (section == TRV_SEC_FX) {
        if (param == 0) {
            *out = (float)s_fx_group;
            return true;
        }
        const FxParamDef* fp = &k_fx_groups[s_fx_group].params[param - 1];
        return fx_read_value(graph, nodes, s_fx_group, fp, out);
    }
    if (section < 0 || section >= TRV_SEC_COUNT) return false;
    const TrvDef* d = &k_trv_sections[section].params[param];
    return trv_read_field(d->field, out);
}

static void trv_write(int section, int param, SituationAudioGraph* graph,
                      const Nodes* nodes, float v)
{
    if (section == TRV_SEC_FX) {
        if (param == 0) {
            int g = (int)(v + 0.5f);
            if (g < 0) g = 0;
            if (g >= FX_GRP_COUNT) g = FX_GRP_COUNT - 1;
            s_fx_group = g;
            if (s_travel_param >= trv_section_param_count(TRV_SEC_FX))
                s_travel_param = trv_section_param_count(TRV_SEC_FX) - 1;
            return;
        }
        const FxParamDef* fp = &k_fx_groups[s_fx_group].params[param - 1];
        fx_write_value(graph, nodes, s_fx_group, fp, v);
        return;
    }
    const TrvDef* d = &k_trv_sections[section].params[param];
    trv_write_field(graph, nodes, d->field, v);
}

static const TrvDef* trv_def(int section, int param)
{
    if (section >= 0 && section < TRV_SEC_FX)
        return &k_trv_sections[section].params[param];
    return NULL;
}

static void trv_clamp_focus(void)
{
    if (s_travel_section < 0) s_travel_section = 0;
    if (s_travel_section >= TRV_SEC_COUNT) s_travel_section = TRV_SEC_COUNT - 1;
    int n = trv_section_param_count(s_travel_section);
    if (n < 1) n = 1;
    if (s_travel_param < 0) s_travel_param = 0;
    if (s_travel_param >= n) s_travel_param = n - 1;
}

static int trv_wrap_mode(int mode, int min_mode, int max_mode, int dir)
{
    if (max_mode < min_mode) return min_mode;
    int span = max_mode - min_mode + 1;
    mode += dir;
    while (mode < min_mode) mode += span;
    while (mode > max_mode) mode -= span;
    return mode;
}

static void trv_adjust_fx(SituationAudioGraph* graph, const Nodes* nodes, int p, int dir, bool fine)
{
    if (p == 0) {
        int g = s_fx_group + dir;
        if (g < 0) g = FX_GRP_COUNT - 1;
        if (g >= FX_GRP_COUNT) g = 0;
        s_fx_group = g;
        trv_clamp_focus();
        return;
    }

    const FxParamDef* fp = &k_fx_groups[s_fx_group].params[p - 1];
    float v = 0.0f;
    if (!fx_read_value(graph, nodes, s_fx_group, fp, &v))
        return;

    if (fp->is_enum) {
        int mode = trv_wrap_mode((int)(v + 0.5f), 0, fp->enum_max, dir);
        fx_write_value(graph, nodes, s_fx_group, fp, (float)mode);
        return;
    }

    if (fp->log_scale) {
        float mul = fine ? ((dir > 0) ? 1.04f : 0.96f) : ((dir > 0) ? 1.12f : 0.89f);
        v = clampf(v * mul, fp->min_val, fp->max_val);
    } else {
        float step = fine ? fp->fine_step : fp->step;
        v = clampf(v + (float)dir * step, fp->min_val, fp->max_val);
    }
    fx_write_value(graph, nodes, s_fx_group, fp, v);
}

static void trv_adjust(SituationAudioGraph* graph, const Nodes* nodes, int dir, bool fine)
{
    int sec = s_travel_section;
    int p = s_travel_param;

    if (sec == TRV_SEC_FX) {
        trv_adjust_fx(graph, nodes, p, dir, fine);
        return;
    }

    const TrvDef* d = trv_def(sec, p);
    if (!d || d->kind == TRV_KIND_ACTION)
        return;

    float v = 0.0f;
    if (!trv_read_field(d->field, &v))
        return;

    if (d->kind == TRV_KIND_BOOL) {
        trv_write_field(graph, nodes, d->field, (dir > 0) ? 1.0f : 0.0f);
        return;
    }

    if (d->kind == TRV_KIND_ENUM) {
        int min_mode = 0;
        int max_mode = d->enum_max;
        if (d->field == TRV_F_FILTER_POLES) {
            min_mode = 1;
            max_mode = 4;
        }
        int mode = trv_wrap_mode((int)(v + 0.5f), min_mode, max_mode, dir);
        trv_write_field(graph, nodes, d->field, (float)mode);
        return;
    }

    if (d->kind == TRV_KIND_LOG) {
        float mul = fine ? ((dir > 0) ? 1.04f : 0.96f) : ((dir > 0) ? 1.12f : 0.89f);
        v = clampf(v * mul, d->min_val, d->max_val);
    } else {
        float step = fine ? d->fine_step : d->step;
        v = clampf(v + (float)dir * step, d->min_val, d->max_val);
    }
    trv_write_field(graph, nodes, d->field, v);
}

static void trv_on_enter(SituationAudioGraph* graph, const Nodes* nodes)
{
    int sec = s_travel_section;
    int p = s_travel_param;

    if (sec == TRV_SEC_FX) {
        if (p == 0) {
            s_fx_group = (s_fx_group + 1) % FX_GRP_COUNT;
            trv_clamp_focus();
            return;
        }
        const FxParamDef* fp = &k_fx_groups[s_fx_group].params[p - 1];
        if (!fp->is_enum) return;
        float v = 0.0f;
        if (!fx_read_value(graph, nodes, s_fx_group, fp, &v)) return;
        int mode = (int)(v + 0.5f);
        mode = (mode + 1) % (fp->enum_max + 1);
        fx_write_value(graph, nodes, s_fx_group, fp, (float)mode);
        return;
    }

    const TrvDef* d = trv_def(sec, p);
    if (!d) return;

    if (d->kind == TRV_KIND_ACTION && d->field == TRV_F_PATCH_STORE) {
        trv_store_patch(graph, nodes);
        return;
    }
    if (d->kind == TRV_KIND_BOOL) {
        float v = 0.0f;
        (void)trv_read_field(d->field, &v);
        trv_write_field(graph, nodes, d->field, (v >= 0.5f) ? 0.0f : 1.0f);
        return;
    }
    if (d->kind != TRV_KIND_ENUM) return;

    float v = 0.0f;
    if (!trv_read_field(d->field, &v)) return;
    int mode = (int)(v + 0.5f);
    mode = (mode + 1) % (d->enum_max + 1);
    trv_write_field(graph, nodes, d->field, (float)mode);
}

static const char* trv_enum_label(int section, const TrvDef* d, int mode)
{
    if (d->field == TRV_F_WAVEFORM || d->field == TRV_F_SUB_WAVE)
        return k_waveform_labels[mode];
    if (d->field == TRV_F_VOICE_MODE)
        return k_voice_mode_labels[mode];
    if (d->field == TRV_F_FILTER_MODE)
        return k_filter_mode_labels[mode];
    if (d->field == TRV_F_FILTER_POLES)
        return k_slope_labels[mode - 1];
    if (d->field == TRV_F_SUB_OCTAVE)
        return k_sub_oct_labels[mode];
    if (d->field == TRV_F_LFO_WAVE)
        return k_lfo_wave_labels[mode];
    if (d->field == TRV_F_FILTER_OS)
        return k_filter_os_labels[mode];
    (void)section;
    return "?";
}

/* Left/Right repeat paced at fixed Hz — frame-rate independent (5 steps/s while held). */
#define TRV_ADJ_RATE_HZ 10.0f

typedef struct {
    int   dir;
    bool  fine;
    int   sec;
    int   param;
    float phase;
} TrvAdjClock;

static TrvAdjClock s_trv_adj_clock;

static void trv_adj_clock_reset(TrvAdjClock* c)
{
    c->dir = 0;
    c->fine = false;
    c->sec = -1;
    c->param = -1;
    c->phase = 0.0f;
}

static void trv_scan_lr_paced(SituationAudioGraph* graph, const Nodes* nodes)
{
    bool fine = SituationIsKeyDown(SIT_KEY_LEFT_SHIFT) || SituationIsKeyDown(SIT_KEY_RIGHT_SHIFT);
    int dir = 0;
    if (SituationIsKeyDown(SIT_KEY_LEFT))  dir = -1;
    if (SituationIsKeyDown(SIT_KEY_RIGHT)) dir = +1;

    if (dir == 0) {
        trv_adj_clock_reset(&s_trv_adj_clock);
        return;
    }

    const int sec = s_travel_section;
    const int param = s_travel_param;
    TrvAdjClock* c = &s_trv_adj_clock;

    if (dir != c->dir || fine != c->fine || sec != c->sec || param != c->param) {
        c->dir = dir;
        c->fine = fine;
        c->sec = sec;
        c->param = param;
        c->phase = 0.0f;
        trv_adjust(graph, nodes, dir, fine);
        return;
    }

    float dt = SituationGetFrameTime();
    if (dt < 0.0f) dt = 0.0f;
    if (dt > 0.25f) dt = 0.25f;
    c->phase += dt;

    const float period = 1.0f / TRV_ADJ_RATE_HZ;
    int steps = 0;
    while (c->phase >= period && steps < 4) {
        trv_adjust(graph, nodes, dir, fine);
        c->phase -= period;
        steps++;
    }
}

static void scan_travel_edit(SituationAudioGraph* graph, const Nodes* nodes)
{
    if (SituationIsKeyPressed(SIT_KEY_F1))
        s_travel_edit = !s_travel_edit;

    if (!s_travel_edit) {
        trv_adj_clock_reset(&s_trv_adj_clock);
        return;
    }

    trv_clamp_focus();

    if (SituationIsKeyPressed(SIT_KEY_PAGE_DOWN)) {
        s_travel_section = (s_travel_section + 1) % TRV_SEC_COUNT;
        s_travel_param = 0;
    }
    if (SituationIsKeyPressed(SIT_KEY_PAGE_UP)) {
        s_travel_section = (s_travel_section + TRV_SEC_COUNT - 1) % TRV_SEC_COUNT;
        s_travel_param = 0;
    }

    int n = trv_section_param_count(s_travel_section);
    if (SituationIsKeyPressed(SIT_KEY_DOWN))
        s_travel_param = (s_travel_param + 1) % n;
    if (SituationIsKeyPressed(SIT_KEY_UP))
        s_travel_param = (s_travel_param + n - 1) % n;
    if (SituationIsKeyPressed(SIT_KEY_HOME))
        s_travel_param = 0;
    if (SituationIsKeyPressed(SIT_KEY_END))
        s_travel_param = n - 1;

    if (SituationIsKeyPressed(SIT_KEY_ENTER))
        trv_on_enter(graph, nodes);

    trv_scan_lr_paced(graph, nodes);
}

static void draw_travel_panel(SituationCommandBuffer cmd, SituationAudioGraph* graph,
                              const Nodes* nodes, int section,
                              float x, float y0, float quad_w, float quad_h,
                              float inner_w, float pad)
{
    const float FS_H  = 12.0f;
    const float FS    = 11.0f;
    const float ROW_H = 15.0f;
    ColorRGBA C_KEY = {180, 185, 220, 200};
    ColorRGBA C_LBL = {160, 165, 200, 220};
    ColorRGBA C_ON  = {80, 220, 120, 255};
    ColorRGBA C_DIM = {120, 125, 155, 200};
    Vector4 sel_row = {{0.22f, 0.28f, 0.38f, 1.0f}};

    const bool active = (section == s_travel_section);
    txt(cmd, active ? "PARAM EDIT" : k_trv_section_titles[section],
        x + pad, y0 + 5.0f, FS_H, active ? C_ON : C_DIM);
    float y = y0 + 22.0f;

    if (!active) {
        txt(cmd, "(PgUp/Dn)", x + pad, y, FS, C_DIM);
        panel_hints(cmd, x + pad, y, inner_w, y0 + quad_h, C_KEY,
                    "F1 off when done", NULL);
        (void)graph; (void)nodes; (void)quad_w; (void)quad_h;
        return;
    }

    {
        static const char* sec_names[TRV_SEC_COUNT];
        for (int i = 0; i < TRV_SEC_COUNT; i++)
            sec_names[i] = k_trv_section_titles[i];
        txt(cmd, "Section  (PgUp/Dn)", x + pad, y, FS, C_LBL);
        y += 11.0f;
        y = draw_btn_row(cmd, x + pad, y, inner_w, sec_names, TRV_SEC_COUNT, s_travel_section,
                         (Vector4){{0.30f, 0.45f, 0.65f, 1.0f}},
                         (Vector4){{0.12f, 0.13f, 0.18f, 1.0f}},
                         (ColorRGBA){220, 235, 255, 255}, (ColorRGBA){120, 125, 155, 200});
        y += 6.0f;
    }

    txt(cmd, "Parameter  (Up/Dn)", x + pad, y, FS, C_LBL);
    y += 11.0f;

    trv_clamp_focus();
    int n = trv_section_param_count(section);
    for (int i = 0; i < n; i++) {
        float val = 0.0f;
        (void)trv_read(section, i, graph, nodes, &val);

        if (i == s_travel_param)
            draw_rect(cmd, x + pad - 2.0f, y - 1.0f, inner_w + 4.0f, ROW_H, sel_row);

        if (section == TRV_SEC_FX && i == 0) {
            int g = (int)(val + 0.5f);
            if (g < 0) g = 0;
            if (g >= FX_GRP_COUNT) g = FX_GRP_COUNT - 1;
            char line[48];
            snprintf(line, sizeof line, "Effect  %s", k_fx_groups[g].name);
            ColorRGBA fc = (i == s_travel_param) ? (ColorRGBA){220, 235, 255, 255}
                                                   : (ColorRGBA){160, 165, 200, 220};
            txt(cmd, line, x + pad + 2.0f, y + 1.0f, 10.5f, fc);
        } else if (section == TRV_SEC_FX) {
            const FxParamDef* fp = &k_fx_groups[s_fx_group].params[i - 1];
            if (fp->is_enum) {
                int mode = (int)(val + 0.5f);
                if (mode < 0) mode = 0;
                if (mode > fp->enum_max) mode = fp->enum_max;
                char line[48];
                snprintf(line, sizeof line, "%s  %s", fp->label, k_overdrive_mode_labels[mode]);
                ColorRGBA fc = (i == s_travel_param) ? (ColorRGBA){255, 220, 255, 255}
                                                     : (ColorRGBA){160, 165, 200, 220};
                txt(cmd, line, x + pad + 2.0f, y + 1.0f, 10.5f, fc);
            } else {
                ColorRGBA bar = (i == s_travel_param) ? (ColorRGBA){220, 120, 255, 255}
                                                      : (ColorRGBA){140, 100, 180, 255};
                param_bar(cmd, fp->label, val, fp->min_val, fp->max_val, x + pad, y, inner_w, bar);
            }
        } else {
            const TrvDef* d = trv_def(section, i);
            if (d->kind == TRV_KIND_ACTION) {
                char line[48];
                snprintf(line, sizeof line, "%s  [Enter save]", d->label);
                ColorRGBA fc = (i == s_travel_param) ? (ColorRGBA){220, 235, 255, 255}
                                                       : (ColorRGBA){160, 165, 200, 220};
                txt(cmd, line, x + pad + 2.0f, y + 1.0f, 10.5f, fc);
            } else if (d->kind == TRV_KIND_BOOL) {
                char line[48];
                snprintf(line, sizeof line, "%s  %s", d->label, (val >= 0.5f) ? "ON" : "OFF");
                ColorRGBA fc = (i == s_travel_param) ? (ColorRGBA){220, 235, 255, 255}
                                                       : (ColorRGBA){160, 165, 200, 220};
                txt(cmd, line, x + pad + 2.0f, y + 1.0f, 10.5f, fc);
            } else if (d->kind == TRV_KIND_ENUM) {
                int mode = (int)(val + 0.5f);
                if (mode < 0) mode = 0;
                if (mode > d->enum_max) mode = d->enum_max;
                if (d->field == TRV_F_FILTER_POLES) {
                    if (mode < 1) mode = 1;
                    if (mode > 4) mode = 4;
                }
                const char* el = trv_enum_label(section, d, mode);
                if (d->field == TRV_F_FILTER_POLES)
                    el = k_slope_labels[mode - 1];
                char line[48];
                snprintf(line, sizeof line, "%s  %s", d->label, el);
                ColorRGBA fc = (i == s_travel_param) ? (ColorRGBA){220, 235, 255, 255}
                                                       : (ColorRGBA){160, 165, 200, 220};
                txt(cmd, line, x + pad + 2.0f, y + 1.0f, 10.5f, fc);
            } else {
                ColorRGBA bar = (i == s_travel_param) ? (ColorRGBA){120, 180, 255, 255}
                                                       : (ColorRGBA){80, 120, 180, 255};
                if (d->field == TRV_F_HOLD && val < 0.0f) {
                    char line[48];
                    snprintf(line, sizeof line, "%s  inf", d->label);
                    ColorRGBA fc = (i == s_travel_param) ? (ColorRGBA){220, 235, 255, 255}
                                                           : (ColorRGBA){160, 165, 200, 220};
                    txt(cmd, line, x + pad + 2.0f, y + 1.0f, 10.5f, fc);
                } else {
                    param_bar(cmd, d->label, val, d->min_val, d->max_val, x + pad, y, inner_w, bar);
                }
            }
        }
        y += ROW_H;
    }

    panel_hints(cmd, x + pad, y, inner_w, y0 + quad_h, C_KEY,
                "F1 off   L/R hold @10/s   Shift fine",
                "choices: one step/tick   Enter save");
    (void)quad_w;
    (void)quad_h;
}
