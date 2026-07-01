# Example 21 — Reference study notes

**Policy:** Visual inspiration only. No Shadertoy GLSL pasted here.

## Design pillars (user)

- **Clouds** — important; water reflections must read cloud shapes
- **Calm water** — as interesting as waves; chop→0 is a feature, not a bug
- **Camera travel** — default experience; orbit is override

## 4sXGRM (primary — study first)

- URL: https://www.shadertoy.com/view/4sXGRM — catalog **oceanic**
- Title / author: *(capture from browser @ O-0c — page not auto-fetched)*
- Why ranked best: *(fill after viewing)*
- Cloud behaviour: layered horizon read; silver lining at sun; shadows on sea
- Wave / calm limit: still water must show deliberate micro-structure + mirror reflect
- Colour / specular: deep navy → shallow turquoise; tight glint when calm, sparkle when chop high
- Camera / framing: low travel sells scale; clouds occupy upper third

## 4dSBDt (Enscape Cube — Thomas Schander)

- URL: https://www.shadertoy.com/view/4dSBDt
- Atmosphere / mood: cinematic sky–sea colour harmony; premium viz framing
- Borrowed in original form: horizon fog tint matched to cloud base; exposure headroom

## MdXyzX (fast open-water)

- URL: https://www.shadertoy.com/view/MdXyzX
- Tiered march idea: coarse interval then refine hit
- Golden-angle wave summation: radial emitters at GA spacing — **implemented** in `seaHeightBase`
- Calm limit: amplitude scales to zero; separate high-freq ripple pass for normals

## v1 realism shipped (original GLSL)

| Feature | Status |
| ------- | ------ |
| Procedural cloud layer + sun silver lining | ✓ |
| Cloud shadow on water | ✓ |
| Golden-angle radial waves (5 emitters + swell) | ✓ |
| Tiered height-field ray march (6 coarse + 3 refine) | ✓ |
| Surface-origin cloud reflections (not camera-origin) | ✓ |
| Calm micro-ripples + mirror reflect branch | ✓ |
| Schlick Fresnel + chop-coupled specular | ✓ |
| Beer–Lambert absorption + horizon fog | ✓ |
| Camera travel path (~90 s loop, lower altitude) | ✓ |

## Wins vs mood refs *(fill after screenshots)*

- 4sXGRM:
- 4dSBDt:
- MdXyzX: