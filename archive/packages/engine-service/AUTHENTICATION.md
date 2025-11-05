# Orpheus Service Driver Authentication

The Orpheus Service Driver supports optional token-based authentication to secure access to the API.

## Quick Start

### Enable Authentication

Start `orpheusd` with the `--auth-token` flag:

```bash
orpheusd --auth-token "your-secret-token-here"
```

**Security Note:** Always use a strong, randomly generated token in production. The token should be kept secret and never committed to version control.

## Usage

### HTTP Requests

Include the token in the `Authorization` header with the `Bearer` scheme:

```bash
curl -X POST http://127.0.0.1:8080/command \
  -H "Authorization: Bearer your-secret-token-here" \
  -H "Content-Type: application/json" \
  -d '{"type":"LoadSession","payload":{"sessionPath":"session.json"}}'
```

### WebSocket Connections

WebSocket connections are authenticated during the initial HTTP upgrade request:

```javascript
const ws = new WebSocket('ws://127.0.0.1:8080/ws', {
  headers: {
    'Authorization': 'Bearer your-secret-token-here'
  }
});
```

Browser WebSocket API doesn't support custom headers, so you need to use a library like `ws` (Node.js) or handle authentication via query parameters (not recommended for production).

## Public Endpoints

The following endpoints are **always public** and do not require authentication:

- `GET /health` - Health check
- `GET /version` - Version information

These endpoints are useful for monitoring and diagnostics without exposing credentials.

## Protected Endpoints

When authentication is enabled, the following endpoints require a valid token:

- `GET /contract` - Contract schema information
- `POST /command` - Command execution
- `GET /ws` - WebSocket event streaming

## Error Responses

### Missing Authorization Header

**Status:** 401 Unauthorized

```json
{
  "error": {
    "code": "UNAUTHORIZED",
    "message": "Missing Authorization header. Use: Authorization: Bearer <token>"
  }
}
```

### Invalid Token Format

**Status:** 401 Unauthorized

```json
{
  "error": {
    "code": "UNAUTHORIZED",
    "message": "Invalid Authorization format. Use: Authorization: Bearer <token>"
  }
}
```

### Invalid Token

**Status:** 401 Unauthorized

```json
{
  "error": {
    "code": "UNAUTHORIZED",
    "message": "Invalid authentication token"
  }
}
```

## Security Best Practices

### Token Generation

Generate strong random tokens using a cryptographically secure method:

```bash
# Generate a random 32-byte token (hex encoded)
openssl rand -hex 32
```

Or in Node.js:

```javascript
import { randomBytes } from 'crypto';
const token = randomBytes(32).toString('hex');
console.log(token);
```

### Environment Variables

Store tokens in environment variables or secure configuration files:

```bash
# .env (never commit this file!)
ORPHEUS_AUTH_TOKEN=your-generated-token-here
```

```bash
# Start the server
orpheusd --auth-token "$ORPHEUS_AUTH_TOKEN"
```

### Network Security

Even with authentication:

1. **Bind to localhost (127.0.0.1)** for local-only access
2. Use **HTTPS/TLS** if exposing to a network
3. Use **firewall rules** to restrict access
4. **Rotate tokens** periodically
5. **Monitor logs** for authentication failures

### Development vs Production

**Development:**
```bash
# Simple token for local testing
orpheusd --auth-token "dev-token-123"
```

**Production:**
```bash
# Strong random token from environment
export ORPHEUS_AUTH_TOKEN=$(openssl rand -hex 32)
orpheusd --auth-token "$ORPHEUS_AUTH_TOKEN"
```

## Logging

Authentication events are logged for security monitoring:

```
[INFO] Token authentication enabled
[WARN] Authentication failed: missing Authorization header
[WARN] Authentication failed: invalid token format
[WARN] Authentication failed: invalid token
[DEBUG] Authentication successful
```

Set log level to `debug` to see successful authentication attempts:

```bash
orpheusd --auth-token "token" --log-level debug
```

## Disabling Authentication

To run without authentication (default):

```bash
orpheusd
```

**Warning:** Only run without authentication on localhost or in secure, isolated environments.

## Testing

### Test Authentication with cURL

```bash
# Test without token (should fail)
curl http://127.0.0.1:8080/contract

# Test with invalid token (should fail)
curl -H "Authorization: Bearer wrong-token" \
  http://127.0.0.1:8080/contract

# Test with valid token (should succeed)
curl -H "Authorization: Bearer your-token" \
  http://127.0.0.1:8080/contract

# Public endpoints work without token
curl http://127.0.0.1:8080/health
```

### Test WebSocket Authentication

```javascript
const WebSocket = require('ws');

// With authentication
const ws = new WebSocket('ws://127.0.0.1:8080/ws', {
  headers: {
    'Authorization': 'Bearer your-token'
  }
});

ws.on('open', () => {
  console.log('Connected!');
});

ws.on('error', (error) => {
  console.error('Connection failed:', error.message);
});
```

## Integration Examples

### JavaScript/TypeScript

```typescript
const token = process.env.ORPHEUS_AUTH_TOKEN;

// HTTP client
const response = await fetch('http://127.0.0.1:8080/command', {
  method: 'POST',
  headers: {
    'Authorization': `Bearer ${token}`,
    'Content-Type': 'application/json'
  },
  body: JSON.stringify({
    type: 'LoadSession',
    payload: { sessionPath: 'session.json' }
  })
});

// WebSocket client (Node.js)
import WebSocket from 'ws';
const ws = new WebSocket('ws://127.0.0.1:8080/ws', {
  headers: { 'Authorization': `Bearer ${token}` }
});
```

### Python

```python
import os
import requests
import websocket

token = os.environ['ORPHEUS_AUTH_TOKEN']
headers = {'Authorization': f'Bearer {token}'}

# HTTP request
response = requests.post(
    'http://127.0.0.1:8080/command',
    headers=headers,
    json={'type': 'LoadSession', 'payload': {'sessionPath': 'session.json'}}
)

# WebSocket (using websocket-client library)
ws = websocket.create_connection(
    'ws://127.0.0.1:8080/ws',
    header=[f'Authorization: Bearer {token}']
)
```

### curl

```bash
TOKEN="your-token-here"

curl -H "Authorization: Bearer $TOKEN" \
  -X POST http://127.0.0.1:8080/command \
  -H "Content-Type: application/json" \
  -d '{"type":"LoadSession","payload":{"sessionPath":"session.json"}}'
```

## Troubleshooting

### Connection Refused

Ensure the server is running and bound to the correct host/port.

### Authentication Always Fails

- Check that the token matches exactly (no extra spaces or newlines)
- Verify the `Authorization` header format: `Bearer <token>`
- Check server logs for specific authentication failure reasons

### WebSocket Upgrade Fails

- Ensure the `Authorization` header is sent during the initial HTTP upgrade
- Some WebSocket clients don't support custom headers (browser native API)
- Consider using a library like `ws` (Node.js) for full header support

## See Also

- [Service Driver README](./README.md) - General usage and configuration
- [Contract Package](../contract/README.md) - Command and event schemas
- [ORP068 Implementation Plan](../../docs/integration/ORP068%20Implementation%20Plan%20v2.0_%20Orpheus%20SDK%20×%20Shmui%20Integration%20.md) - Full integration roadmap
