#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <unordered_set>

#include "cpphttp.hpp"

// Thread-safe session storage
std::unordered_set<std::string> active_sessions;
std::mutex sessions_mutex;

// Simple helper to generate a secure random session token
std::string GenerateSessionToken() {
  static const char charset[] =
      "0123456789"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz";
  const size_t max_index = sizeof(charset) - 1;
  std::random_device rd;
  std::mt19937 generator(rd());
  std::uniform_int_distribution<size_t> distribution(0, max_index - 1);
  std::string token;
  token.reserve(32);
  for (size_t i = 0; i < 32; ++i) {
    token += charset[distribution(generator)];
  }
  return token;
}

// Simple cookie value extractor
std::string GetCookieValue(const std::string &cookie_header,
                           const std::string &name) {
  size_t pos = 0;
  while (pos < cookie_header.size()) {
    size_t next_semi = cookie_header.find(';', pos);
    std::string pair = cookie_header.substr(pos, next_semi == std::string::npos
                                                     ? std::string::npos
                                                     : next_semi - pos);

    // Trim leading and trailing whitespace
    size_t first = pair.find_first_not_of(" \t");
    if (first != std::string::npos) {
      size_t last = pair.find_last_not_of(" \t");
      pair = pair.substr(first, last - first + 1);
    } else {
      pair.clear();
    }

    size_t eq = pair.find('=');
    if (eq != std::string::npos) {
      std::string key = pair.substr(0, eq);
      std::string value = pair.substr(eq + 1);
      if (key == name) {
        return value;
      }
    }
    if (next_semi == std::string::npos) break;
    pos = next_semi + 1;
  }
  return "";
}

// Simple helper to extract value from JSON format like {"key": "value"}
std::string ExtractJsonString(const std::string &json, const std::string &key) {
  size_t key_pos = json.find("\"" + key + "\"");
  if (key_pos == std::string::npos) return "";
  size_t colon_pos = json.find(":", key_pos);
  if (colon_pos == std::string::npos) return "";
  size_t first_quote = json.find("\"", colon_pos);
  if (first_quote == std::string::npos) return "";
  size_t second_quote = json.find("\"", first_quote + 1);
  if (second_quote == std::string::npos) return "";
  return json.substr(first_quote + 1, second_quote - first_quote - 1);
}

int main() {
  std::string spa_dir = "./demo_spa_dist";
  try {
    // Create a dummy SPA directory structure for the example
    std::filesystem::create_directories(spa_dir);
    std::filesystem::create_directories(spa_dir + "/static/css");

    {
      std::ofstream out(spa_dir + "/index.html");
      out << R"html(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>🚀 SPA Portal</title>
  <link href="https://fonts.googleapis.com/css2?family=Plus+Jakarta+Sans:wght@300;400;500;600;700;800&display=swap" rel="stylesheet">
  <link rel="stylesheet" href="/app/static/css/style.css">
</head>
<body>
  <div class="glow-bg"></div>
  
  <div id="auth-container" class="auth-container hidden">
    <!-- Login form injected here -->
  </div>

  <div id="app-container" class="container hidden">
    <header class="header">
      <div class="logo">🚀 SPAPortal</div>
      <nav class="nav">
        <a href="/app/" class="nav-link">Home</a>
        <a href="/app/dashboard" class="nav-link">Dashboard</a>
        <a href="/app/profile" class="nav-link">Profile</a>
      </nav>
      <div class="user-pill">
        <span class="user-badge" id="username-badge">Admin</span>
        <button class="btn btn-secondary logout-btn" id="logout-btn">Logout</button>
      </div>
    </header>
    
    <main class="main-content" id="app-content">
      <!-- Content injected by JS router -->
    </main>
  </div>

  <script>
    let user = null;

    async function checkAuth() {
      try {
        const res = await fetch('/api/me');
        const data = await res.json();
        if (data.authenticated) {
          user = data.username;
          document.getElementById('username-badge').textContent = user;
          document.getElementById('app-container').classList.remove('hidden');
          document.getElementById('auth-container').classList.add('hidden');
          return true;
        }
      } catch (err) {
        console.error('Auth check failed:', err);
      }
      user = null;
      document.getElementById('app-container').classList.add('hidden');
      document.getElementById('auth-container').classList.remove('hidden');
      return false;
    }

    async function login(username, password) {
      const errorContainer = document.getElementById('login-error');
      if (errorContainer) {
        errorContainer.classList.add('hidden');
        errorContainer.innerHTML = '';
      }
      
      try {
        const res = await fetch('/api/login', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ username, password })
        });
        const data = await res.json();
        if (data.success) {
          const authenticated = await checkAuth();
          if (authenticated) {
            navigate('/app/');
          }
        } else {
          showLoginError(data.error || 'Login failed');
        }
      } catch (err) {
        showLoginError('Network error or server unavailable');
      }
    }

    function showLoginError(msg) {
      const container = document.getElementById('login-error');
      if (container) {
        container.textContent = msg;
        container.classList.remove('hidden');
        container.classList.remove('shake');
        void container.offsetWidth; // Force reflow
        container.classList.add('shake');
      }
    }

    async function logout() {
      try {
        await fetch('/api/logout', { method: 'POST' });
      } catch (err) {
        console.error('Logout error:', err);
      }
      await checkAuth();
      navigate('/app/login');
    }

    function navigate(path) {
      window.history.pushState(null, '', path);
      render();
    }

    const views = {
      home: () => `
        <div class="welcome-card">
          <h1>Welcome back to SPAPortal 🚀</h1>
          <p>This is a modern C++ powered Single Page Application featuring cookie-based session authentication, server-side route filtering middleware, and dynamic data binding.</p>
        </div>
        <div class="grid-3">
          <div class="card">
            <h3>🛡️ Session Cookies</h3>
            <p>Sessions are managed using HttpOnly, SameSite=Strict cookies to defend against Cross-Site Scripting (XSS) and Request Forgery (CSRF).</p>
          </div>
          <div class="card">
            <h3>⚙️ C++ Middleware</h3>
            <p>The backend HttpServer interceptor protects the API routes, returning 401 Unauthorized for invalid session tokens.</p>
          </div>
          <div class="card">
            <h3>⚡ Client Router</h3>
            <p>Client routing captures navigation and fallbacks gracefully, querying user sessions asynchronously without blocking the UI.</p>
          </div>
        </div>
      `,
      dashboard: (data) => {
        const logsHtml = (data.logs || []).map(log => `
          <div class="log-item">
            <span class="log-time">${log.time}</span>
            <span class="log-event">${log.event}</span>
            <span class="log-level ${log.level}">${log.level}</span>
          </div>
        `).join('');
        
        return `
          <div class="welcome-card" style="padding: 1.5rem 2.5rem; margin-bottom: 1.5rem; background: linear-gradient(135deg, rgba(99,102,241,0.08) 0%, rgba(168,85,247,0.08) 100%);">
            <h2 style="font-size: 1.75rem; font-weight: 800;">System Dashboard (Simulated Telemetry)</h2>
            <p style="font-size: 0.95rem; color: var(--text-muted);">Real-time simulated server telemetry and mock authentication session logs</p>
          </div>
          <div class="grid-3">
            <div class="card">
              <h3>Active Sessions</h3>
              <div class="metric-value">${data.activeSessions || 0}</div>
              <p>Unique authenticated browser contexts</p>
            </div>
            <div class="card">
              <h3>Server Uptime (Simulated)</h3>
              <div class="metric-value" style="font-size: 1.8rem; height: 3.4rem; display: flex; align-items: center;">${data.uptime || 'N/A'}</div>
              <p>Continuous thread process uptime (Simulated)</p>
            </div>
            <div class="card">
              <h3>Telemetry Requests (Simulated)</h3>
              <div class="metric-value">${data.totalRequests || 0}</div>
              <p>Served HTTP requests processed (Simulated)</p>
            </div>
          </div>
          
          <div class="card activity-logs">
            <h3 style="margin-bottom: 1rem; border-bottom: 1px solid var(--border); padding-bottom: 0.5rem;">🔒 Recent Security & Access Events (Simulated Logs)</h3>
            <div id="logs-container">
              ${logsHtml || '<p style="color: var(--text-muted);">No recent logs</p>'}
            </div>
          </div>
        `;
      },
      profile: () => `
        <div class="welcome-card" style="padding: 1.5rem 2.5rem; margin-bottom: 1.5rem; background: linear-gradient(135deg, rgba(99,102,241,0.08) 0%, rgba(168,85,247,0.08) 100%);">
          <h2 style="font-size: 1.75rem; font-weight: 800;">User Account Profile</h2>
          <p style="font-size: 0.95rem; color: var(--text-muted);">Security context of the current session (Simulated Profile)</p>
        </div>
        <div class="card" style="max-width: 600px; margin: 0 auto;">
          <div style="text-align: center; margin-bottom: 2rem;">
            <div style="width: 80px; height: 80px; background: var(--accent-gradient); border-radius: 50%; display: inline-flex; align-items: center; justify-content: center; font-size: 2rem; font-weight: 800; box-shadow: 0 8px 24px rgba(99, 102, 241, 0.3); margin-bottom: 1rem;">A</div>
            <h3>admin</h3>
            <span class="user-badge">Role: Administrator</span>
          </div>
          <div class="profile-card">
            <div class="profile-field">
              <span class="profile-label">Authentication Type</span>
              <span class="profile-value">Stateful Token Cookie</span>
            </div>
            <div class="profile-field">
              <span class="profile-label">Storage Integrity</span>
              <span class="profile-value">Memory Resident</span>
            </div>
          </div>
        </div>
      `,
      login: () => `
        <div class="auth-card">
          <div style="font-size: 3rem; margin-bottom: 0.5rem;">🚀</div>
          <h2>SPA Portal</h2>
          <p>Log in to view system telemetry</p>
          
          <div id="login-error" class="error-badge hidden"></div>
          
          <form id="login-form">
            <div class="form-group">
              <label for="username">USERNAME</label>
              <input type="text" id="username" class="form-input" placeholder="admin" required autocomplete="username">
            </div>
            <div class="form-group">
              <label for="password">PASSWORD</label>
              <input type="password" id="password" class="form-input" placeholder="••••••••" required autocomplete="current-password">
            </div>
            <button type="submit" class="btn btn-primary">Sign In</button>
          </form>
          
          <div class="auth-info-badge">
            Use credentials:<br>
            Username: <code>admin</code><br>
            Password: <code>password123</code>
          </div>
        </div>
      `
    };

    async function render() {
      const path = window.location.pathname;
      
      // Set navbar active states
      document.querySelectorAll('.nav-link').forEach(link => {
        if (link.getAttribute('href') === path) {
          link.classList.add('active');
        } else {
          link.classList.remove('active');
        }
      });

      if (!user) {
        if (path !== '/app/login') {
          window.history.replaceState(null, '', '/app/login');
        }
        const container = document.getElementById('auth-container');
        container.innerHTML = views.login();
        document.getElementById('app-container').classList.add('hidden');
        container.classList.remove('hidden');
        
        // Attach form handler
        document.getElementById('login-form').addEventListener('submit', async (e) => {
          e.preventDefault();
          const u = document.getElementById('username').value.trim();
          const p = document.getElementById('password').value;
          await login(u, p);
        });
        return;
      }

      // If user is authenticated and tries to hit /app/login, redirect to /app/
      if (path === '/app/login') {
        navigate('/app/');
        return;
      }

      const appContent = document.getElementById('app-content');
      
      if (path === '/app/' || path === '/app') {
        appContent.innerHTML = views.home();
      } else if (path === '/app/dashboard') {
        appContent.innerHTML = `<div style="text-align: center; padding: 3rem;"><p style="color: var(--text-muted);">Loading telemetry data...</p></div>`;
        try {
          const response = await fetch('/api/dashboard-data');
          if (response.status === 401) {
            await checkAuth();
            render();
            return;
          }
          const data = await response.json();
          appContent.innerHTML = views.dashboard(data);
        } catch (e) {
          appContent.innerHTML = `<div class="card" style="text-align: center; border-color: var(--danger);"><h3 style="color: var(--danger);">Error</h3><p>Failed to load dashboard statistics.</p></div>`;
        }
      } else if (path === '/app/profile') {
        appContent.innerHTML = views.profile();
      } else {
        appContent.innerHTML = `
          <div class="card" style="text-align: center; padding: 4rem 2rem;">
            <h1 style="font-size: 4rem; margin-bottom: 1rem; color: var(--accent);">404</h1>
            <h3>Page Not Found</h3>
            <p style="color: var(--text-muted); margin-bottom: 2rem; margin-top: 0.5rem;">The client-side router fallback served index.html, but this path is not registered.</p>
            <button class="btn btn-primary" onclick="navigate('/app/')">Go Home</button>
          </div>
        `;
      }
    }

    // Event Listeners
    document.addEventListener('click', e => {
      if (e.target.matches('.nav-link')) {
        e.preventDefault();
        navigate(e.target.getAttribute('href'));
      }
    });

    document.getElementById('logout-btn').addEventListener('click', logout);
    window.addEventListener('popstate', render);

    // Initial bootstrap
    (async () => {
      await checkAuth();
      render();
    })();
  </script>
</body>
</html>
)html";
    }

    {
      std::ofstream out(spa_dir + "/static/css/style.css");
      out << R"css(:root {
  --bg: #0b0d19;
  --panel: rgba(30, 41, 59, 0.45);
  --border: rgba(255, 255, 255, 0.08);
  --accent: #6366f1;
  --accent-hover: #4f46e5;
  --accent-gradient: linear-gradient(135deg, #6366f1, #a855f7);
  --text: #f3f4f6;
  --text-muted: #94a3b8;
  --success: #10b981;
  --danger: #f43f5e;
  --card-bg: rgba(15, 23, 42, 0.6);
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
  justify-content: center;
  overflow-x: hidden;
}

.glow-bg {
  position: absolute;
  width: 600px;
  height: 600px;
  background: radial-gradient(circle, rgba(99, 102, 241, 0.15) 0%, rgba(168, 85, 247, 0.05) 50%, transparent 100%);
  top: -200px;
  left: 50%;
  transform: translateX(-50%);
  z-index: -1;
  filter: blur(100px);
  pointer-events: none;
}

.hidden {
  display: none !important;
}

.container {
  width: 100%;
  max-width: 1000px;
  padding: 2rem;
  animation: fadeIn 0.4s cubic-bezier(0.16, 1, 0.3, 1) forwards;
}

.header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 1rem 1.5rem;
  background: var(--panel);
  border: 1px solid var(--border);
  backdrop-filter: blur(16px);
  border-radius: 16px;
  margin-bottom: 2.5rem;
  box-shadow: 0 10px 30px rgba(0, 0, 0, 0.2);
}

.logo {
  font-size: 1.25rem;
  font-weight: 800;
  background: var(--accent-gradient);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
}

.nav {
  display: flex;
  gap: 1.5rem;
}

.nav-link {
  color: var(--text-muted);
  text-decoration: none;
  font-weight: 600;
  font-size: 0.95rem;
  transition: all 0.2s ease;
  padding: 0.5rem 0.75rem;
  border-radius: 8px;
}

.nav-link:hover, .nav-link.active {
  color: var(--text);
  background: rgba(255, 255, 255, 0.05);
}

.user-pill {
  display: flex;
  align-items: center;
  gap: 1rem;
}

.user-badge {
  font-size: 0.85rem;
  font-weight: 600;
  background: rgba(99, 102, 241, 0.15);
  color: #a5b4fc;
  padding: 0.35rem 0.75rem;
  border-radius: 9999px;
  border: 1px solid rgba(99, 102, 241, 0.3);
}

.btn {
  font-family: inherit;
  font-size: 0.9rem;
  font-weight: 600;
  padding: 0.6rem 1.2rem;
  border-radius: 10px;
  border: none;
  cursor: pointer;
  transition: all 0.2s ease;
}

.btn-primary {
  background: var(--accent-gradient);
  color: white;
  box-shadow: 0 4px 15px rgba(99, 102, 241, 0.3);
}

.btn-primary:hover {
  transform: translateY(-2px);
  box-shadow: 0 6px 20px rgba(99, 102, 241, 0.4);
}

.btn-secondary {
  background: rgba(255, 255, 255, 0.08);
  color: var(--text);
  border: 1px solid var(--border);
}

.btn-secondary:hover {
  background: rgba(255, 255, 255, 0.15);
}

/* Auth Portal */
.auth-container {
  display: flex;
  align-items: center;
  justify-content: center;
  min-height: 100vh;
  width: 100%;
  padding: 1rem;
}

.auth-card {
  width: 100%;
  max-width: 420px;
  background: var(--panel);
  border: 1px solid var(--border);
  backdrop-filter: blur(20px);
  padding: 2.5rem;
  border-radius: 24px;
  box-shadow: 0 20px 40px rgba(0, 0, 0, 0.3);
  text-align: center;
  animation: scaleIn 0.4s cubic-bezier(0.16, 1, 0.3, 1) forwards;
}

.auth-card h2 {
  font-size: 1.75rem;
  font-weight: 800;
  margin-bottom: 0.5rem;
  letter-spacing: -0.025em;
}

.auth-card p {
  color: var(--text-muted);
  font-size: 0.95rem;
  margin-bottom: 2rem;
}

.form-group {
  text-align: left;
  margin-bottom: 1.25rem;
}

.form-group label {
  display: block;
  font-size: 0.85rem;
  font-weight: 600;
  color: var(--text-muted);
  margin-bottom: 0.5rem;
}

.form-input {
  width: 100%;
  background: rgba(0, 0, 0, 0.2);
  border: 1px solid var(--border);
  padding: 0.75rem 1rem;
  border-radius: 10px;
  color: var(--text);
  font-family: inherit;
  font-size: 0.95rem;
  transition: all 0.2s ease;
}

.form-input:focus {
  outline: none;
  border-color: var(--accent);
  box-shadow: 0 0 0 3px rgba(99, 102, 241, 0.15);
}

.auth-card .btn {
  width: 100%;
  margin-top: 1rem;
  padding: 0.8rem;
}

.auth-info-badge {
  background: rgba(255, 255, 255, 0.03);
  border: 1px solid var(--border);
  padding: 0.75rem;
  border-radius: 12px;
  margin-top: 1.5rem;
  font-size: 0.8rem;
  color: var(--text-muted);
  line-height: 1.5;
}

.auth-info-badge code {
  color: #a5b4fc;
  font-weight: 600;
}

.error-badge {
  background: rgba(244, 63, 94, 0.15);
  border: 1px solid rgba(244, 63, 94, 0.3);
  color: #fca5a5;
  padding: 0.75rem;
  border-radius: 10px;
  font-size: 0.85rem;
  margin-bottom: 1.5rem;
  text-align: left;
  animation: shake 0.3s ease-in-out;
}

/* Animations */
@keyframes fadeIn {
  from { opacity: 0; transform: translateY(15px); }
  to { opacity: 1; transform: translateY(0); }
}

@keyframes scaleIn {
  from { opacity: 0; transform: scale(0.95); }
  to { opacity: 1; transform: scale(1); }
}

@keyframes shake {
  0%, 100% { transform: translateX(0); }
  25% { transform: translateX(-6px); }
  75% { transform: translateX(6px); }
}

/* Main Content Views */
.welcome-card {
  background: linear-gradient(135deg, rgba(99, 102, 241, 0.1) 0%, rgba(168, 85, 247, 0.1) 100%);
  border: 1px solid rgba(168, 85, 247, 0.2);
  padding: 2.5rem;
  border-radius: 20px;
  margin-bottom: 2rem;
}

.welcome-card h1 {
  font-size: 2.25rem;
  font-weight: 800;
  margin-bottom: 1rem;
}

.welcome-card p {
  font-size: 1.1rem;
  color: #cbd5e1;
  line-height: 1.6;
}

.grid-3 {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
  gap: 1.5rem;
  margin-bottom: 2rem;
}

.card {
  background: var(--card-bg);
  border: 1px solid var(--border);
  padding: 1.75rem;
  border-radius: 16px;
  box-shadow: 0 4px 20px rgba(0, 0, 0, 0.15);
  transition: all 0.3s ease;
}

.card:hover {
  transform: translateY(-4px);
  border-color: rgba(255, 255, 255, 0.15);
}

.card h3 {
  font-size: 1.2rem;
  margin-bottom: 0.75rem;
  color: var(--text);
}

.card p {
  color: var(--text-muted);
  line-height: 1.5;
  font-size: 0.95rem;
}

/* Dashboard styles */
.metric-value {
  font-size: 2.25rem;
  font-weight: 800;
  background: var(--accent-gradient);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
  margin: 0.5rem 0;
}

.activity-logs {
  margin-top: 2rem;
}

.log-item {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 0.75rem 1rem;
  background: rgba(255, 255, 255, 0.02);
  border: 1px solid var(--border);
  border-radius: 10px;
  margin-bottom: 0.5rem;
  font-size: 0.9rem;
}

.log-time {
  color: var(--accent);
  font-family: monospace;
  font-weight: 600;
}

.log-event {
  flex: 1;
  margin-left: 1.5rem;
}

.log-level {
  font-size: 0.75rem;
  font-weight: 700;
  padding: 0.2rem 0.5rem;
  border-radius: 4px;
  text-transform: uppercase;
}

.log-level.info {
  background: rgba(16, 185, 129, 0.15);
  color: #34d399;
}

.log-level.warning {
  background: rgba(245, 158, 11, 0.15);
  color: #fbbf24;
}

.profile-card {
  display: flex;
  flex-direction: column;
  gap: 1rem;
  margin-top: 1rem;
}

.profile-field {
  display: flex;
  justify-content: space-between;
  padding: 0.75rem 0;
  border-bottom: 1px solid rgba(255, 255, 255, 0.05);
}

.profile-label {
  color: var(--text-muted);
  font-weight: 600;
}

.profile-value {
  color: var(--text);
  font-family: monospace;
}
)css";
    }

    // Run the HTTP server
    uint16_t port = 8085;
    cpphttp::HttpServer server(port);

    // Add authentication middleware for all API routes except login and me
    // status check
    server.Use("/api/", [](cpphttp::HttpRequest &req,
                           cpphttp::HttpResponse &res) {
      if (req.path == "/api/login" || req.path == "/api/me") {
        return true;  // Bypass authentication check for login and me status
                      // check
      }

      std::string cookie_header = req.GetHeader("Cookie");
      std::string token = GetCookieValue(cookie_header, "session_token");

      bool authenticated = false;
      {
        std::lock_guard<std::mutex> lock(sessions_mutex);
        if (!token.empty() &&
            active_sessions.find(token) != active_sessions.end()) {
          authenticated = true;
        }
      }

      if (!authenticated) {
        res = cpphttp::HttpResponse::Json("{\"error\": \"Unauthorized\"}", 401);
        return false;  // Interrupt pipeline
      }

      return true;
    });

    // POST /api/login
    server.Post("/api/login", [](const cpphttp::HttpRequest &req) {
      std::string username = ExtractJsonString(req.body, "username");
      std::string password = ExtractJsonString(req.body, "password");

      if (username == "admin" && password == "password123") {
        std::string token = GenerateSessionToken();
        {
          std::lock_guard<std::mutex> lock(sessions_mutex);
          active_sessions.insert(token);
        }

        cpphttp::HttpResponse res =
            cpphttp::HttpResponse::Json("{\"success\": true}", 200);
        res.headers["Set-Cookie"] =
            "session_token=" + token + "; Path=/; HttpOnly; SameSite=Strict";
        return res;
      }

      return cpphttp::HttpResponse::Json(
          "{\"success\": false, \"error\": \"Invalid username or password\"}",
          400);
    });

    // POST /api/logout
    server.Post("/api/logout", [](const cpphttp::HttpRequest &req) {
      std::string cookie_header = req.GetHeader("Cookie");
      std::string token = GetCookieValue(cookie_header, "session_token");

      if (!token.empty()) {
        std::lock_guard<std::mutex> lock(sessions_mutex);
        active_sessions.erase(token);
      }

      cpphttp::HttpResponse res =
          cpphttp::HttpResponse::Json("{\"success\": true}", 200);
      res.headers["Set-Cookie"] =
          "session_token=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0";
      return res;
    });

    // GET /api/me
    server.Get("/api/me", [](const cpphttp::HttpRequest &req) {
      std::string cookie_header = req.GetHeader("Cookie");
      std::string token = GetCookieValue(cookie_header, "session_token");

      bool authenticated = false;
      {
        std::lock_guard<std::mutex> lock(sessions_mutex);
        if (!token.empty() &&
            active_sessions.find(token) != active_sessions.end()) {
          authenticated = true;
        }
      }

      if (authenticated) {
        return cpphttp::HttpResponse::Json(
            "{\"authenticated\": true, \"username\": \"admin\"}", 200);
      } else {
        return cpphttp::HttpResponse::Json("{\"authenticated\": false}", 200);
      }
    });

    // GET /api/dashboard-data
    server.Get("/api/dashboard-data", [](const cpphttp::HttpRequest &req) {
      size_t sessions_count = 0;
      {
        std::lock_guard<std::mutex> lock(sessions_mutex);
        sessions_count = active_sessions.size();
      }

      std::ostringstream json;
      json << "{"
           << "\"uptime\": \"2 hours, 14 mins\","
           << "\"activeSessions\": " << sessions_count << ","
           << "\"totalRequests\": 421,"
           << "\"logs\": ["
           << "  {\"time\": \"18:40:02\", \"event\": \"User admin logged in "
              "successfully\", \"level\": \"info\"},"
           << "  {\"time\": \"18:38:15\", \"event\": \"Failed login attempt "
              "from "
              "client IP 127.0.0.1\", \"level\": \"warning\"},"
           << "  {\"time\": \"18:30:00\", \"event\": \"Server started on port "
              "8080\", \"level\": \"info\"}"
           << "]"
           << "}";
      return cpphttp::HttpResponse::Json(json.str(), 200);
    });

    // Serve static files with SPA mode enabled
    server.StaticDir("/app", spa_dir, true);

    std::cout << "Starting SPA example server on port " << port << "...\n";
    std::cout << "Visit http://localhost:" << port
              << "/app/ in your browser.\n";

    server.Start();

    // Keep the main thread alive while the background server thread runs
    std::cout << "Press Enter to exit..." << std::endl;
    std::cin.get();

    server.Stop();

    // Cleanup
    std::filesystem::remove_all(spa_dir);
  } catch (const std::exception &e) {
    std::cerr << "Fatal Exception in main: " << e.what() << std::endl;
    std::error_code ec;
    std::filesystem::remove_all(spa_dir, ec);
    return 1;
  }

  return 0;
}
