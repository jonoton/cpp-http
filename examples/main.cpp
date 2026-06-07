#include "cpphttp.hpp"
#include <iostream>

const char* UI_HTML = R"html(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>C++ Secure WebSocket Demo</title>
  <link href="https://fonts.googleapis.com/css2?family=Plus+Jakarta+Sans:wght@300;400;500;600;700&family=JetBrains+Mono:wght@400;500&display=swap" rel="stylesheet">
  <style>
    :root {
      --bg: #0b0d19;
      --panel: rgba(20, 24, 46, 0.6);
      --border: rgba(255, 255, 255, 0.08);
      --accent: #6366f1;
      --accent-hover: #4f46e5;
      --accent-glow: rgba(99, 102, 241, 0.3);
      --text: #f3f4f6;
      --text-muted: #9ca3af;
      --success: #10b981;
      --danger: #ef4444;
    }
    * {
      box-sizing: border-box;
      margin: 0;
      padding: 0;
    }
    body {
      font-family: 'Plus Jakarta Sans', sans-serif;
      background: radial-gradient(circle at top right, #1e1b4b, var(--bg));
      color: var(--text);
      min-height: 100vh;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      padding: 2rem;
      overflow-x: hidden;
    }
    .container {
      width: 100%;
      max-width: 800px;
      backdrop-filter: blur(16px);
      background: var(--panel);
      border: 1px solid var(--border);
      border-radius: 24px;
      box-shadow: 0 20px 40px rgba(0, 0, 0, 0.3);
      display: flex;
      flex-direction: column;
      height: 600px;
      overflow: hidden;
      transition: transform 0.3s ease, box-shadow 0.3s ease;
    }
    .header {
      padding: 1.5rem 2rem;
      border-bottom: 1px solid var(--border);
      display: flex;
      align-items: center;
      justify-content: space-between;
    }
    .title-area {
      display: flex;
      align-items: center;
      gap: 0.75rem;
    }
    .logo-dot {
      width: 12px;
      height: 12px;
      background: var(--accent);
      border-radius: 50%;
      box-shadow: 0 0 12px var(--accent);
      animation: pulse 2s infinite;
    }
    h1 {
      font-size: 1.25rem;
      font-weight: 700;
      letter-spacing: -0.025em;
    }
    .status-badge {
      display: flex;
      align-items: center;
      gap: 0.5rem;
      font-size: 0.875rem;
      font-weight: 500;
      padding: 0.375rem 0.875rem;
      background: rgba(255, 255, 255, 0.05);
      border: 1px solid var(--border);
      border-radius: 9999px;
    }
    .status-dot {
      width: 8px;
      height: 8px;
      background: var(--text-muted);
      border-radius: 50%;
    }
    .status-dot.connected {
      background: var(--success);
      box-shadow: 0 0 8px var(--success);
    }
    .status-dot.disconnected {
      background: var(--danger);
      box-shadow: 0 0 8px var(--danger);
    }
    .messages-container {
      flex: 1;
      padding: 2rem;
      overflow-y: auto;
      display: flex;
      flex-direction: column;
      gap: 1rem;
      background: rgba(0, 0, 0, 0.1);
    }
    .message {
      max-width: 80%;
      padding: 0.875rem 1.25rem;
      border-radius: 16px;
      font-size: 0.95rem;
      line-height: 1.5;
      animation: slideIn 0.3s cubic-bezier(0.16, 1, 0.3, 1) forwards;
      opacity: 0;
      transform: translateY(10px);
    }
    .message.sent {
      align-self: flex-end;
      background: linear-gradient(135deg, var(--accent), var(--accent-hover));
      color: white;
      border-bottom-right-radius: 4px;
      box-shadow: 0 4px 12px var(--accent-glow);
    }
    .message.received {
      align-self: flex-start;
      background: rgba(255, 255, 255, 0.06);
      border: 1px solid var(--border);
      color: var(--text);
      border-bottom-left-radius: 4px;
    }
    .message.system {
      align-self: center;
      background: rgba(255, 255, 255, 0.03);
      border: 1px solid var(--border);
      color: var(--text-muted);
      font-size: 0.8rem;
      padding: 0.375rem 0.75rem;
      border-radius: 9999px;
      font-family: 'JetBrains Mono', monospace;
    }
    .message.error {
      align-self: center;
      background: rgba(239, 68, 68, 0.1);
      border: 1px solid rgba(239, 68, 68, 0.25);
      color: #f87171;
      font-size: 0.85rem;
      padding: 0.5rem 1rem;
      border-radius: 12px;
      font-family: 'JetBrains Mono', monospace;
      max-width: 90%;
      text-align: center;
      box-shadow: 0 4px 12px rgba(239, 68, 68, 0.1);
    }
    .input-area {
      padding: 1.5rem 2rem;
      border-top: 1px solid var(--border);
      display: flex;
      gap: 1rem;
    }
    input {
      flex: 1;
      background: rgba(0, 0, 0, 0.2);
      border: 1px solid var(--border);
      border-radius: 12px;
      padding: 0.875rem 1.25rem;
      color: var(--text);
      font-family: inherit;
      font-size: 0.95rem;
      outline: none;
      transition: border-color 0.2s, box-shadow 0.2s;
    }
    input:focus {
      border-color: var(--accent);
      box-shadow: 0 0 0 3px var(--accent-glow);
    }
    button {
      background: var(--accent);
      color: white;
      border: none;
      border-radius: 12px;
      padding: 0 1.5rem;
      font-family: inherit;
      font-size: 0.95rem;
      font-weight: 600;
      cursor: pointer;
      transition: background 0.2s, transform 0.1s;
    }
    button:hover {
      background: var(--accent-hover);
    }
    button:active {
      transform: scale(0.98);
    }
    .controls-bar {
      padding: 0.75rem 2rem;
      border-bottom: 1px solid var(--border);
      background: rgba(0, 0, 0, 0.15);
      display: flex;
      align-items: center;
      gap: 1.5rem;
      flex-wrap: wrap;
      transition: all 0.3s ease;
    }
    .control-group {
      display: flex;
      align-items: center;
      gap: 0.75rem;
      transition: opacity 0.3s ease, transform 0.3s ease;
    }
    .checkbox-container {
      display: flex;
      align-items: center;
      gap: 0.5rem;
      cursor: pointer;
      user-select: none;
      font-size: 0.9rem;
      color: var(--text-muted);
      transition: color 0.2s;
    }
    .checkbox-container:hover {
      color: var(--text);
    }
    .checkbox-container input[type="checkbox"] {
      cursor: pointer;
      accent-color: var(--accent);
      width: 16px;
      height: 16px;
    }
    .btn-sm {
      padding: 0.5rem 1rem;
      font-size: 0.85rem;
      border-radius: 8px;
    }
    .btn-secondary {
      background: rgba(255, 255, 255, 0.05);
      border: 1px solid var(--border);
      color: var(--text);
    }
    .btn-secondary:hover {
      background: rgba(255, 255, 255, 0.10);
    }
    .btn-secondary:disabled {
      opacity: 0.5;
      cursor: not-allowed;
    }
    .btn-danger {
      background: var(--danger);
      color: white;
    }
    .btn-danger:hover {
      background: #dc2626;
    }
    .btn-danger:disabled {
      opacity: 0.5;
      cursor: not-allowed;
    }
    .cert-info {
      margin-top: 1.5rem;
      text-align: center;
      font-size: 0.85rem;
      color: var(--text-muted);
      max-width: 600px;
      line-height: 1.4;
    }
    .cert-info a {
      color: var(--accent);
      text-decoration: none;
    }
    .cert-info a:hover {
      text-decoration: underline;
    }
    @keyframes pulse {
      0%, 100% { transform: scale(1); opacity: 1; }
      50% { transform: scale(1.2); opacity: 0.6; }
    }
    @keyframes slideIn {
      to { opacity: 1; transform: translateY(0); }
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <div class="title-area">
        <div class="logo-dot"></div>
        <h1>Secure WebSocket Console</h1>
      </div>
      <div class="status-badge">
        <div id="statusDot" class="status-dot disconnected"></div>
        <span id="statusText">Disconnected</span>
      </div>
    </div>
    <div class="controls-bar">
      <label class="checkbox-container">
        <input type="checkbox" id="autoReconnectCheck" checked>
        <span>Auto Reconnect</span>
      </label>
      <div id="manualControls" class="control-group" style="display: none; opacity: 0; transform: translateY(-5px);">
        <button id="connectBtn" class="btn-sm btn-secondary">Connect</button>
        <button id="disconnectBtn" class="btn-sm btn-danger" disabled>Disconnect</button>
      </div>
    </div>
    <div id="messages" class="messages-container">
    </div>
    <div class="input-area">
      <input type="text" id="messageInput" placeholder="Type a message to echo..." disabled>
      <button id="sendBtn" disabled>Send</button>
    </div>
  </div>
  <div class="cert-info">
    Running over HTTPS. If WebSocket fails to connect, please ensure you have accepted the self-signed certificate by visiting <a href="https://localhost:8080/" target="_blank">https://localhost:8080/</a> and clicking "Advanced" -> "Proceed".
  </div>

  <script>
    const messagesDiv = document.getElementById('messages');
    const input = document.getElementById('messageInput');
    const button = document.getElementById('sendBtn');
    const statusDot = document.getElementById('statusDot');
    const statusText = document.getElementById('statusText');

    const autoReconnectCheck = document.getElementById('autoReconnectCheck');
    const manualControls = document.getElementById('manualControls');
    const connectBtn = document.getElementById('connectBtn');
    const disconnectBtn = document.getElementById('disconnectBtn');

    let socket = null;
    let reconnectTimeout = null;
    let reconnectInterval = null;
    let connectionTimeout = null;
    let isManuallyClosed = false;

    function addMessage(text, type) {
      const msg = document.createElement('div');
      msg.classList.add('message', type);
      msg.textContent = text;
      messagesDiv.appendChild(msg);
      messagesDiv.scrollTop = messagesDiv.scrollHeight;
    }

    const wsUrl = `wss://${window.location.host}/ws`;

    function connect() {
      if (socket && (socket.readyState === WebSocket.CONNECTING || socket.readyState === WebSocket.OPEN)) {
        return;
      }

      if (reconnectTimeout) {
        clearTimeout(reconnectTimeout);
        reconnectTimeout = null;
      }
      if (reconnectInterval) {
        clearInterval(reconnectInterval);
        reconnectInterval = null;
      }
      if (connectionTimeout) {
        clearTimeout(connectionTimeout);
        connectionTimeout = null;
      }

      isManuallyClosed = false;
      addMessage(`System: Attempting to connect to ${wsUrl}...`, 'system');
      statusDot.className = 'status-dot disconnected';
      statusText.textContent = 'Connecting...';
      connectBtn.disabled = true;
      disconnectBtn.disabled = true;

      try {
        socket = new WebSocket(wsUrl);
      } catch (err) {
        addMessage('System: Failed to create WebSocket connection: ' + err.message, 'error');
        handleDisconnect();
        return;
      }

      // If the connection doesn't open within 3.5 seconds, close it and retry.
      // This avoids hanging in the CONNECTING state (especially when proxy/port-forwarding is active).
      connectionTimeout = setTimeout(() => {
        if (socket && socket.readyState === WebSocket.CONNECTING) {
          addMessage('System: Connection attempt timed out. Retrying...', 'error');
          socket.close();
        }
      }, 3500);

      socket.onopen = () => {
        if (connectionTimeout) {
          clearTimeout(connectionTimeout);
          connectionTimeout = null;
        }
        statusDot.className = 'status-dot connected';
        statusText.textContent = 'Connected';
        input.disabled = false;
        button.disabled = false;
        input.focus();
        connectBtn.disabled = true;
        disconnectBtn.disabled = false;
        addMessage('System: Secure WebSocket connection established successfully.', 'system');
      };

      socket.onmessage = (event) => {
        addMessage(event.data, 'received');
      };

      socket.onclose = (event) => {
        handleDisconnect(event);
      };

      socket.onerror = (error) => {
        console.error('WebSocket error:', error);
      };
    }

    function disconnect() {
      isManuallyClosed = true;
      if (reconnectTimeout) {
        clearTimeout(reconnectTimeout);
        reconnectTimeout = null;
      }
      if (reconnectInterval) {
        clearInterval(reconnectInterval);
        reconnectInterval = null;
      }
      if (connectionTimeout) {
        clearTimeout(connectionTimeout);
        connectionTimeout = null;
      }
      if (socket) {
        socket.close();
      }
    }

    function handleDisconnect(event) {
      if (connectionTimeout) {
        clearTimeout(connectionTimeout);
        connectionTimeout = null;
      }
      if (reconnectInterval) {
        clearInterval(reconnectInterval);
        reconnectInterval = null;
      }
      statusDot.className = 'status-dot disconnected';
      statusText.textContent = 'Disconnected';
      input.disabled = true;
      button.disabled = true;
      connectBtn.disabled = false;
      disconnectBtn.disabled = true;

      let closeMsg = 'System: WebSocket connection closed.';
      let isError = false;

      if (event) {
        const code = event.code;
        const reason = event.reason;

        if (code === 1000) {
          closeMsg = `System: Connection closed cleanly (Code ${code}).`;
        } else if (code === 1001) {
          closeMsg = `System: Connection closed: Server went offline (Code ${code}).`;
          isError = true;
        } else if (code === 1006) {
          closeMsg = `System: Connection closed abnormally (Code ${code}). Ensure the server is running and the self-signed SSL certificate is accepted.`;
          isError = true;
        } else if (code === 1009) {
          closeMsg = `System: Connection closed: Message too big (Code ${code}).`;
          if (reason) {
            closeMsg += ` Reason: "${reason}"`;
          }
          isError = true;
        } else if (code) {
          closeMsg = `System: Connection closed (Code ${code}).`;
          if (reason) {
            closeMsg += ` Reason: "${reason}"`;
          }
          if (code !== 1005) {
            isError = true;
          }
        }
      }

      addMessage(closeMsg, isError ? 'error' : 'system');

      // If auto reconnect is enabled and NOT manually closed, retry
      if (autoReconnectCheck.checked && !isManuallyClosed) {
        let secondsLeft = 3;
        addMessage(`System: Reconnecting in ${secondsLeft} seconds...`, 'system');
        const countdownMsgEl = messagesDiv.lastElementChild;

        reconnectInterval = setInterval(() => {
          secondsLeft--;
          if (secondsLeft > 0) {
            countdownMsgEl.textContent = `System: Reconnecting in ${secondsLeft} seconds...`;
          } else {
            clearInterval(reconnectInterval);
            reconnectInterval = null;
          }
        }, 1000);

        reconnectTimeout = setTimeout(() => {
          if (reconnectInterval) {
            clearInterval(reconnectInterval);
            reconnectInterval = null;
          }
          connect();
        }, 3000);
      }
    }

    function updateManualControlsVisibility() {
      if (autoReconnectCheck.checked) {
        manualControls.style.opacity = '0';
        manualControls.style.transform = 'translateY(-5px)';
        setTimeout(() => {
          if (autoReconnectCheck.checked) {
            manualControls.style.display = 'none';
          }
        }, 300);
      } else {
        manualControls.style.display = 'flex';
        // Force reflow
        manualControls.offsetHeight;
        manualControls.style.opacity = '1';
        manualControls.style.transform = 'translateY(0)';
      }
    }

    autoReconnectCheck.addEventListener('change', () => {
      updateManualControlsVisibility();
      if (autoReconnectCheck.checked) {
        // If we enable auto-reconnect and we are disconnected, attempt to connect
        if (!socket || socket.readyState === WebSocket.CLOSED) {
          connect();
        }
      } else {
        // If we disable auto-reconnect, clear any active reconnect timeout
        if (reconnectTimeout) {
          clearTimeout(reconnectTimeout);
          reconnectTimeout = null;
          addMessage('System: Auto-reconnect disabled.', 'system');
        }
        if (reconnectInterval) {
          clearInterval(reconnectInterval);
          reconnectInterval = null;
        }
      }
    });

    connectBtn.addEventListener('click', connect);
    disconnectBtn.addEventListener('click', disconnect);

    function sendMessage() {
      const text = input.value.trim();
      if (text && socket && socket.readyState === WebSocket.OPEN) {
        socket.send(text);
        addMessage(text, 'sent');
        input.value = '';
        input.focus();
      }
    }

    button.addEventListener('click', sendMessage);
    input.addEventListener('keydown', (e) => {
      if (e.key === 'Enter') {
        sendMessage();
      }
    });

    // Start connection on page load
    connect();
  </script>
</body>
</html>
)html";

int main() {
  // Construct HttpServer on port 8080
  cpphttp::HttpServer server(8080);

  // Configure SSL certificates from certs/ directory
  cpptcpnet::TcpListener::SslConfig ssl_config;
  ssl_config.cert_file = "/workspaces/cpp-http/certs/server.crt";
  ssl_config.key_file = "/workspaces/cpp-http/certs/server.key";
  try {
    server.GetListener().EnableSSL(ssl_config);
    std::cout << "SSL support enabled." << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Failed to enable SSL: " << e.what() << std::endl;
    return 1;
  }

  // Register a simple GET route to serve the rich Web UI
  server.Get("/", [](const cpphttp::HttpRequest &req) {
    cpphttp::HttpResponse res;
    res.status_code = 200;
    res.status_message = "OK";
    res.headers["Content-Type"] = "text/html";
    res.body = UI_HTML;
    return res;
  });

  // Register a WebSocket echo route
  cpphttp::WebSocketBehavior ws_behavior;
  ws_behavior.on_open = [](std::shared_ptr<cpphttp::WebSocketConnection> conn) {
    std::cout << "WebSocket connection opened: session_id=" << conn->GetSessionId() << std::endl;
  };
  ws_behavior.on_message = [](std::shared_ptr<cpphttp::WebSocketConnection> conn, const std::string &msg) {
    std::cout << "WebSocket received message: " << msg << std::endl;
    conn->Send("Server Echo: " + msg);
  };
  ws_behavior.on_close = [](std::shared_ptr<cpphttp::WebSocketConnection> conn) {
    std::cout << "WebSocket connection closed: session_id=" << conn->GetSessionId() << std::endl;
  };
  server.WebSocket("/ws", std::move(ws_behavior));

  // Start the server
  try {
    server.Start();
    std::cout << "HTTPS server started on port 8080. Press Enter to stop."
              << std::endl;
    std::cin.get();
    server.Stop();
  } catch (const std::exception &e) {
    std::cerr << "Failed to start server: " << e.what() << std::endl;
  }

  return 0;
}
