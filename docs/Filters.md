# CLI filter settings that impact PID controllers

This list focuses on CLI `set` parameters that are filter-related and affect PID behavior directly (filtering PID inputs/terms) or indirectly (filtering pilot/throttle commands that feed PID loops).

Default and allowed-range values are taken from `src/main/fc/settings.yaml`. Where the settings metadata specifies only an upper bound for a non-negative setting, the table shows that as `0-max`.

## Gyro measurement filtering (affects all attitude/rate PID loops)

| CLI setting | Default | Allowed range | PID controller(s) impacted | Impact on PID behavior |
|---|---|---|---|---|
| `gyro_anti_aliasing_lpf_hz` | `250` | `0-1000` | Multirotor/fixed-wing/rover/boat stabilization loops using gyro rate feedback | Sets pre-processing anti-aliasing LPF on gyro data. Lower values suppress high-frequency noise before it reaches P/D calculations, reducing noise-driven oscillations at the cost of additional phase lag. |
| `gyro_lulu_enabled` | `OFF` | `OFF` / `ON` | Same as above | Enables/disables LULU nonlinear gyro filtering. When enabled, spikes/outliers in gyro measurements are reduced, improving noise immunity for P/D paths but potentially softening very fast transients. |
| `gyro_lulu_sample_count` | `3` | `1-15` | Same as above | Controls LULU window size. Higher values increase spike rejection and smoothing, with more latency/softness in fast rate changes seen by PID. |
| `gyro_main_lpf_hz` | `60` | `0-500` | Same as above | Main software gyro LPF cutoff. Lower cutoff gives cleaner gyro feedback and calmer P/D response, but with more delay and reduced sharpness. |
| `gyro_filter_mode` | `STATIC` | `OFF`, `STATIC`, `DYNAMIC`, `ADAPTIVE` | Same as above | Selects static vs dynamic gyro LPF strategy. Dynamic modes vary filtering with operating conditions (typically throttle), trading noise suppression vs latency adaptively. |
| `gyro_dyn_lpf_min_hz` | `200` | `40-400` | Same as above | Minimum cutoff used by dynamic gyro LPF. Lower minimum increases maximum filtering in noisy conditions. |
| `gyro_dyn_lpf_max_hz` | `500` | `40-1000` | Same as above | Maximum cutoff used by dynamic gyro LPF. Higher maximum preserves responsiveness in cleaner/low-throttle conditions. |
| `gyro_dyn_lpf_curve_expo` | `5` | `1-10` | Same as above | Shapes throttle-to-cutoff mapping for dynamic LPF. Changes where in the throttle range filtering gets aggressive, which changes P/D noise-vs-lag balance through flight envelope. |
| `dynamic_gyro_notch_enabled` | `ON` | `OFF` / `ON` | Same as above | Enables dynamic (matrix) notch filtering on gyro data. Removes narrowband motor/prop resonance peaks before PID uses gyro feedback. |
| `dynamic_gyro_notch_q` | `120` | `1-1000` | Same as above | Q for dynamic notch(es). Higher Q = narrower notch (less collateral delay, more precise peak removal); lower Q = wider attenuation (more robust but more phase impact). |
| `dynamic_gyro_notch_min_hz` | `50` | `30-250` | Same as above | Lower frequency floor for dynamic notch tracking. Lets notch follow lower-RPM resonance regions (larger props/lower motor speeds). |
| `dynamic_gyro_notch_mode` | `2D` | `2D`, `3D` | Same as above | Chooses notch topology (e.g., 2D/3D behavior). Affects how many axes/frequencies are tracked and therefore how much resonance cleanup vs computational/filter complexity is applied. |
| `dynamic_gyro_notch_3d_q` | `200` | `1-1000` | Same as above | Q for 3D dynamic notch mode. Tunes selectivity/width specifically for 3D notch operation. |
| `rpm_gyro_filter_enabled` | `OFF` | `OFF` / `ON` | Same as above | Enables RPM-synchronous notch filtering using ESC telemetry. Strongly reduces motor-order noise entering P/D terms when RPM telemetry is valid. |
| `rpm_gyro_harmonics` | `1` | `1-3` | Same as above | Number of motor harmonics to notch. More harmonics remove more tonal noise but add filtering complexity and possible extra phase shift. |
| `rpm_gyro_min_hz` | `100` | `30-200` | Same as above | Minimum RPM-notch frequency. Controls low-end tracking for RPM-based notches. |
| `rpm_gyro_q` | `500` | `1-3000` | Same as above | Q factor for RPM notches. Same width/selectivity tradeoff as other notch Q settings. |
| `gyro_adaptive_filter_target` | `3.5` | `1-6` | Same as above | Target noise metric for adaptive gyro LPF logic. Influences how aggressively adaptive filtering raises/lowers cutoff to keep target noise level. |
| `gyro_adaptive_filter_min_hz` | `50` | `0-250` | Same as above | Minimum adaptive cutoff. Lower values allow heavier filtering (more lag, less noise). |
| `gyro_adaptive_filter_max_hz` | `150` | `0-505` | Same as above | Maximum adaptive cutoff. Higher values allow lower-lag response when noise conditions permit. |
| `gyro_adaptive_filter_std_lpf_hz` | `2` | `0-10` | Same as above | LPF on the adaptive filter’s standard deviation estimator. Higher values make adaptation react faster; lower values smooth adaptation changes. |
| `gyro_adaptive_filter_hpf_hz` | `10` | `1-50` | Same as above | HPF used in adaptive noise measurement path; determines what frequency content is treated as noise when driving adaptive updates. |
| `gyro_adaptive_filter_integrator_threshold_high` | `4` | `1-10` | Same as above | Upper threshold for adaptive integrator action; affects when filter cutoff is pushed toward stronger filtering. |
| `gyro_adaptive_filter_integrator_threshold_low` | `-2` | `-10 to 0` | Same as above | Lower threshold for adaptive integrator action; affects when filter cutoff is relaxed toward responsiveness. |

## PID-term specific filtering (directly filters P/I/D-related signals)

| CLI setting | Default | Allowed range | PID controller(s) impacted | Impact on PID behavior |
|---|---|---|---|---|
| `dterm_lpf_hz` | `110` | `0-500` | Stabilization PID D-term (primarily roll/pitch; platform dependent) | Stage-1 D-term LPF cutoff. Lower values reduce D-term noise and motor heat risk; higher values improve damping immediacy but pass more noise. |
| `dterm_lpf_type` | `PT2` | `PT1`, `BIQUAD`, `PT2`, `PT3`, `LULU` | Same D-term path as above | Chooses D-term LPF filter family/order (`PT1`, `BIQUAD`, `PT2`, `PT3`), trading attenuation steepness vs delay/phase shift. |
| `yaw_lpf_hz` | `0` | `0-200` | Yaw stabilization PID (P-term path per setting description) | Low-pass filtering on yaw control term to prevent high-frequency yaw oscillation/noise sensitivity; too low can make yaw feel delayed. |
| `mc_iterm_relax_cutoff` | `15` | `1-100` | Multirotor stabilization PID I-term relax logic | LPF cutoff used in I-term-relax path. Lower cutoff makes I-term relaxation smoother/stronger against fast stick events; higher cutoff tracks pilot inputs more tightly. |
| `d_boost_gyro_delta_lpf_hz` | `80` | `10-250` | Multirotor D-boost feature feeding D-term gain modulation | LPF on gyro-delta signal that drives D-boost. Lower values smooth boost changes (less jitter), higher values react faster to transients. |
| `nav_mc_vel_xy_dterm_lpf_hz` | `2` | `0-100` | Multirotor horizontal velocity PID (`PID_VEL_XY`) D-term in navigation modes | Filters D-term contribution for position-hold/cruise/WP/RTH speed control at higher travel speeds; lowers twitching but can soften braking sharpness. |
| `fw_i_level` | `5` | `0-255` | Fixed-wing attitude leveling (`PID_LEVEL`) | This parameter is defined as fixed-wing attitude stabilization low-pass filter cutoff (stored in the `I` slot for this legacy controller field). Lower values smooth leveling corrections; higher values allow quicker leveling response. |
| `smith_predictor_lpf_hz` | `50` | `1-500` | Smith-predictor-assisted stabilization loop (when enabled) | LPF on Smith predictor path; lower cutoff smooths prediction signal and can improve robustness to noise, but increases delay in predictor contribution. |

## Command/setpoint filtering that changes PID input demand

| CLI setting | Default | Allowed range | PID controller(s) impacted | Impact on PID behavior |
|---|---|---|---|---|
| `rc_filter_lpf_hz` | `50` | `15-250` | All pilot-driven stabilization loops (rate/angle/horizon) | LPF on RC command input. Lower cutoff smooths stick demand into PID setpoints (less jerk/noise), but increases command latency. |
| `rc_filter_auto` | `ON` | `OFF` / `ON` | Same as above | Enables auto-selection of RC filtering from update rate + smoothing factor, dynamically setting command smoothing behavior that drives PID targets. |
| `rc_filter_smoothing_factor` | `30` | `1-100` | Same as above | Higher smoothing factor increases command filtering and delay; lower factor gives crisper setpoint response into PID loops. |
| `mc_cd_lpf_hz` | `30` | `0-200` | Multirotor Control Derivative / feed-forward path | LPF on control-derivative (feed-forward) signal entering PID path. Lower values make FF smoother/less noisy, higher values keep stick-connected sharpness. |
| `setpoint_kalman_enabled` | `ON` | `OFF` / `ON` | Stabilization loops using setpoint Kalman path | Enables Kalman filtering for setpoint/gyro-related control path, reducing noise in demanded motion trajectory seen by controller. |
| `setpoint_kalman_q` | `100` | `1-1000` | Same as above | Kalman quality factor. Higher `q` usually means less filtering and less delay; lower `q` increases smoothing with more phase delay in commanded response. |
| `antigravity_cutoff_lpf_hz` | `15` | `1-30` | Antigravity modulation of stabilization I-term behavior | LPF cutoff for throttle signal used by antigravity logic. Lower cutoff increases smoothing in throttle-change detection (less abrupt I-term gain modulation), higher cutoff reacts faster to throttle punches/chops. |

## Sensor filtering that affects specific PID modes

| CLI setting | Default | Allowed range | PID controller(s) impacted | Impact on PID behavior |
|---|---|---|---|---|
| `acc_lpf_hz` | `15` | `0-200` | Angle/leveling controllers using accelerometer feedback (`PID_LEVEL` and related attitude estimation consumers) | LPF on accelerometer data. Lower values reduce vibration-induced attitude noise at the expense of slower attitude correction response. |
| `acc_lpf_type` | `BIQUAD` | `PT1`, `BIQUAD` | Same as above | Chooses accelerometer LPF type (e.g., PT1/BIQUAD), trading filtering strength and delay. |
| `acc_notch_hz` | `0` | `0-255` | Same as above | Center frequency for accelerometer notch filter to suppress frame-specific vibration components contaminating level/attitude estimation used by attitude PID loops. |
| `acc_notch_cutoff` | `1` | `1-255` | Same as above | Cutoff/width control for accelerometer notch; determines selectivity and residual phase impact on attitude feedback quality. |

## Filtered control outputs in PI/PID-adjacent loops

| CLI setting | Default | Allowed range | PID controller(s) impacted | Impact on PID behavior |
|---|---|---|---|---|
| `limit_attn_filter_cutoff` | `1.2` | `0-100` | Throttle attenuation PI control loop (power limiting subsystem) | LPF on PI output command for power-limit attenuation. Lower cutoff smooths throttle limiting action and prevents abrupt corrections; higher cutoff makes attenuation react faster. |

