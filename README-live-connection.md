# Inkanini RS485: Wokwi to dashboard

The dashboard now polls `GET /api/telemetry` every 1.2 seconds. The ESP32 sketch posts its current counters, relay state, line status, and most recent event every 1.5 seconds. The API stores the current state and a de-duplicated event log in Upstash Redis so separate Vercel function calls share data.

## Deploy the API with the dashboard

1. In Vercel, connect the Git repository used by `inkanini-rs485.vercel.app` and copy the files from this `outputs` folder into its project root. Keep `index.html` at the root and `api/telemetry.js` under `api/`.
2. In Vercel Marketplace, add an Upstash Redis database to that project. It supplies `UPSTASH_REDIS_REST_URL` and `UPSTASH_REDIS_REST_TOKEN`.
3. Add a Vercel environment variable named `TELEMETRY_DEVICE_TOKEN` with a new random value. Add it to Production (and Preview if needed), then redeploy.
4. In `wokwi-sketch.ino`, replace `REPLACE_WITH_TELEMETRY_DEVICE_TOKEN` with the same value and keep the endpoint URL pointed at the deployed project.
5. Paste the sketch into Wokwi, start the simulation, and watch the dashboard. Press the attack/fault buttons in the simulation; the blocked counter, relay state and event log should update within a few seconds.

## Payload and endpoint

`POST /api/telemetry` requires the `X-Device-Token` header and accepts JSON fields `deviceId`, `blockedAttackCount`, `relayIsolated`, `protectionActive`, `baudRate`, `lineQuality`, `uptimeSeconds`, `voltage`, and `event`. `GET /api/telemetry` returns `{ "state": ..., "events": [...] }`. The browser only reads; it does not send remote relay or maintenance commands to the device.

Wokwi provides the `Wokwi-GUEST` network with outbound internet access. The sketch uses `setInsecure()` for the simulator demo; for physical ESP32 hardware, replace that with certificate validation and protect the device token. Never commit the real token to a public repository. If the Wokwi project is public, use a demo-only token and rotate it after the presentation.

## If your current sketch already has logic

The screenshot shows these same pin constants. If your current sketch has additional functionality, preserve it and port the Wi-Fi constants, `sendTelemetry()` helper, and `setEvent()` calls into your existing sketch rather than replacing your whole program. The provided sketch is a complete matching example, not a copy of any code that was not visible in the screenshot.
