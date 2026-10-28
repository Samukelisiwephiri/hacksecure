# Inkanini RS485: Wokwi live telemetry

GitHub Pages is static hosting: it cannot receive the ESP32's `POST` requests. Sending telemetry to `samukelisiwephiri.github.io/hacksecure/` therefore returns HTTP 405. A serverless API must be deployed separately; this project includes the Vercel function at `api/telemetry.js`.

## Deploy the receiver

1. Import the directory containing `index.html` and `api/telemetry.js` as a Vercel project. If importing the parent repository, set the Vercel Root Directory to `outputs/inkanini-rs485-live-project`.
2. Connect an Upstash Redis database to the Vercel project and make sure `UPSTASH_REDIS_REST_URL` and `UPSTASH_REDIS_REST_TOKEN` are set in its environment.
3. Add `TELEMETRY_DEVICE_TOKEN` as a Vercel environment variable with a new random value, then deploy. Copy the deployment origin Vercel assigns (for example, `https://your-project.vercel.app`). The previously documented `inkanini-rs485.vercel.app` URL is not currently serving this API.
4. Verify `https://your-project.vercel.app/api/telemetry` returns HTTP 200 JSON. An empty store should return `{"state":null,"events":[]}`.

## Point Wokwi and the dashboard at the API

1. In [wokwi-sketch.ino](./wokwi-sketch.ino), replace `YOUR-VERCEL-PROJECT` in `TELEMETRY_URL` with the Vercel project name and replace `REPLACE_WITH_TELEMETRY_DEVICE_TOKEN` with the exact `TELEMETRY_DEVICE_TOKEN` value. Paste the updated sketch into Wokwi. Do not use the GitHub Pages URL for `TELEMETRY_URL`; it must end in `/api/telemetry`.
2. If the dashboard is served by Vercel from this same project, leave the `telemetry-api-origin` meta tag empty. If it remains on GitHub Pages, set that tag in `index.html` to the Vercel origin, for example `<meta name="telemetry-api-origin" content="https://your-project.vercel.app">`, then publish the updated page.
3. Start the Wokwi simulation. Its serial console should show `Telemetry POST: HTTP 200`; the dashboard should change from waiting to `DEVICE LIVE` and show events/counters within a few seconds. HTTP 401 means the token does not match; HTTP 503 means Redis is missing or unavailable.

## Data contract and safety

`POST /api/telemetry` requires the `X-Device-Token` header and accepts `deviceId`, `blockedAttackCount`, `relayIsolated`, `protectionActive`, `baudRate`, `lineQuality`, `uptimeSeconds`, `voltage`, and `event`. `GET /api/telemetry` returns `{ "state": ..., "events": [...] }`. The browser only reads; it does not send remote relay or maintenance commands to the device.

The sketch uses Wokwi's `Wokwi-GUEST` network and `setInsecure()` for the simulator only. For physical ESP32 hardware, validate the server certificate. Never commit a real token to a public repository; public Wokwi projects should use a demo-only token and rotate it after the presentation.

If your current sketch has additional logic, preserve it and port the Wi-Fi constants, `sendTelemetry()` helper, and `setEvent()` calls into it instead of replacing the whole program.
