const cors = {
  'Access-Control-Allow-Origin': '*',
  'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
  'Access-Control-Allow-Headers': 'Content-Type, X-Device-Token',
  'Cache-Control': 'no-store, max-age=0',
};

async function redis(commands) {
  const base = process.env.UPSTASH_REDIS_REST_URL;
  const token = process.env.UPSTASH_REDIS_REST_TOKEN;
  if (!base || !token) throw new Error('Upstash Redis is not configured');
  const response = await fetch(`${base.replace(/\/$/, '')}/pipeline`, {
    method: 'POST',
    headers: { Authorization: `Bearer ${token}`, 'Content-Type': 'application/json' },
    body: JSON.stringify(commands),
  });
  if (!response.ok) throw new Error(`Redis returned ${response.status}`);
  const results = await response.json();
  const failed = results.find((item) => item.error);
  if (failed) throw new Error(failed.error);
  return results.map((item) => item.result);
}

module.exports = async function handler(req, res) {
  for (const [key, value] of Object.entries(cors)) res.setHeader(key, value);
  if (req.method === 'OPTIONS') return res.status(204).end();
  if (req.method === 'GET') {
    try {
      const [rawState, rawEvents] = await redis([
        ['GET', 'inkanini:telemetry:latest'],
        ['LRANGE', 'inkanini:telemetry:events', '0', '29'],
      ]);
      return res.status(200).json({
        state: rawState ? JSON.parse(rawState) : null,
        events: (rawEvents || []).map((item) => JSON.parse(item)),
      });
    } catch (error) {
      console.error('Telemetry read failed:', error.message);
      return res.status(503).json({ error: 'Telemetry storage is unavailable' });
    }
  }
  if (req.method !== 'POST') {
    res.setHeader('Allow', 'GET, POST, OPTIONS');
    return res.status(405).json({ error: 'Method not allowed' });
  }
  if (!process.env.TELEMETRY_DEVICE_TOKEN || req.headers['x-device-token'] !== process.env.TELEMETRY_DEVICE_TOKEN) {
    return res.status(401).json({ error: 'Unauthorized device' });
  }
  try {
    const body = typeof req.body === 'string' ? JSON.parse(req.body) : req.body;
    if (!body || typeof body !== 'object' || Array.isArray(body)) return res.status(400).json({ error: 'JSON object required' });
    const state = {
      deviceId: String(body.deviceId || 'esp32-wokwi').slice(0, 48),
      blockedAttackCount: Math.max(0, Math.floor(Number(body.blockedAttackCount) || 0)),
      relayIsolated: Boolean(body.relayIsolated),
      protectionActive: Boolean(body.protectionActive),
      baudRate: Math.max(0, Math.floor(Number(body.baudRate) || 9600)),
      lineQuality: String(body.lineQuality || 'CLEAN').slice(0, 16),
      uptimeSeconds: Math.max(0, Math.floor(Number(body.uptimeSeconds) || 0)),
      voltage: Number.isFinite(Number(body.voltage)) ? Number(body.voltage) : null,
      timestamp: new Date().toISOString(),
    };
    if (body.event && body.event.id !== undefined) {
      state.event = {
        id: String(body.event.id).slice(0, 80),
        type: String(body.event.type || 'SYS').slice(0, 12).toUpperCase(),
        message: String(body.event.message || 'Device event').slice(0, 240),
      };
    }
    await redis([['SET', 'inkanini:telemetry:latest', JSON.stringify(state)]]);
    if (state.event) {
      const [inserted] = await redis([['SET', `inkanini:telemetry:event:${state.event.id}`, '1', 'EX', '86400', 'NX']]);
      if (inserted === 'OK') await redis([
        ['LPUSH', 'inkanini:telemetry:events', JSON.stringify(state.event)],
        ['LTRIM', 'inkanini:telemetry:events', '0', '29'],
      ]);
    }
    return res.status(200).json({ ok: true, receivedAt: state.timestamp });
  } catch (error) {
    console.error('Telemetry write failed:', error.message);
    return res.status(503).json({ error: 'Telemetry could not be stored' });
  }
};
