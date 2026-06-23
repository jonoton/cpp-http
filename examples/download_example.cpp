#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "cpphttp.hpp"

// Premium HTML interface page
const char *UI_HTML = R"html(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>C++ Static File Download Demo</title>
  <link href="https://fonts.googleapis.com/css2?family=Plus+Jakarta+Sans:wght@300;400;500;600;700&family=JetBrains+Mono:wght@400;500;600&display=swap" rel="stylesheet">
  <style>
    :root {
      --bg: #030712;
      --panel: rgba(17, 24, 39, 0.7);
      --border: rgba(255, 255, 255, 0.08);
      --border-glow: rgba(99, 102, 241, 0.2);
      
      --small-color: #10b981;
      --small-glow: rgba(16, 185, 129, 0.15);
      
      --medium-color: #3b82f6;
      --medium-glow: rgba(59, 130, 246, 0.15);
      
      --large-color: #d946ef;
      --large-glow: rgba(217, 70, 239, 0.15);
      
      --text: #f3f4f6;
      --text-muted: #9ca3af;
      --terminal-bg: #090d16;
    }
    * {
      box-sizing: border-box;
      margin: 0;
      padding: 0;
    }
    body {
      font-family: 'Plus Jakarta Sans', sans-serif;
      background: radial-gradient(circle at 50% 50%, #1e1b4b 0%, var(--bg) 100%);
      color: var(--text);
      min-height: 100vh;
      display: flex;
      flex-direction: column;
      align-items: center;
      padding: 2rem 1rem;
      overflow-x: hidden;
    }
    header {
      text-align: center;
      margin-bottom: 3rem;
      max-width: 800px;
    }
    .badge {
      background: linear-gradient(135deg, #6366f1, #a855f7);
      color: white;
      font-size: 0.75rem;
      font-weight: 700;
      text-transform: uppercase;
      letter-spacing: 0.1em;
      padding: 0.35rem 0.85rem;
      border-radius: 9999px;
      display: inline-block;
      margin-bottom: 1rem;
      box-shadow: 0 4px 12px rgba(99, 102, 241, 0.3);
    }
    h1 {
      font-size: 2.5rem;
      font-weight: 800;
      letter-spacing: -0.03em;
      margin-bottom: 0.75rem;
      background: linear-gradient(to right, #f3f4f6, #9ca3af);
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
    }
    .subtitle {
      font-size: 1.1rem;
      color: var(--text-muted);
      line-height: 1.6;
    }
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
      gap: 2rem;
      width: 100%;
      max-width: 1100px;
      margin-bottom: 3rem;
    }
    .card {
      background: var(--panel);
      backdrop-filter: blur(12px);
      border: 1px solid var(--border);
      border-radius: 20px;
      padding: 2rem;
      display: flex;
      flex-direction: column;
      position: relative;
      transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1);
      overflow: hidden;
    }
    .card::before {
      content: '';
      position: absolute;
      top: 0;
      left: 0;
      right: 0;
      height: 4px;
      transition: all 0.3s ease;
    }
    .card.small::before { background: var(--small-color); }
    .card.medium::before { background: var(--medium-color); }
    .card.large::before { background: var(--large-color); }
    
    .card:hover {
      transform: translateY(-5px);
      border-color: rgba(255, 255, 255, 0.15);
    }
    .card.small:hover { box-shadow: 0 10px 30px var(--small-glow); }
    .card.medium:hover { box-shadow: 0 10px 30px var(--medium-glow); }
    .card.large:hover { box-shadow: 0 10px 30px var(--large-glow); }

    .file-icon {
      font-size: 2.5rem;
      margin-bottom: 1rem;
      display: flex;
      align-items: center;
    }
    .card.small .file-icon { color: var(--small-color); }
    .card.medium .file-icon { color: var(--medium-color); }
    .card.large .file-icon { color: var(--large-color); }

    .file-name {
      font-size: 1.25rem;
      font-weight: 700;
      margin-bottom: 0.5rem;
      word-break: break-all;
    }
    .file-size {
      font-size: 0.9rem;
      font-weight: 600;
      color: var(--text-muted);
      margin-bottom: 1rem;
      display: inline-flex;
      align-items: center;
      gap: 0.5rem;
    }
    .size-tag {
      font-size: 0.75rem;
      padding: 0.2rem 0.5rem;
      border-radius: 4px;
      font-weight: 700;
    }
    .card.small .size-tag { background: var(--small-glow); color: var(--small-color); }
    .card.medium .size-tag { background: var(--medium-glow); color: var(--medium-color); }
    .card.large .size-tag { background: var(--large-glow); color: var(--large-color); }

    .file-desc {
      font-size: 0.95rem;
      color: var(--text-muted);
      line-height: 1.5;
      margin-bottom: 2rem;
      flex-grow: 1;
    }
    .button-group {
      display: flex;
      flex-direction: column;
      gap: 0.75rem;
      margin-bottom: 1.5rem;
    }
    .btn {
      width: 100%;
      padding: 0.8rem 1.2rem;
      font-size: 0.9rem;
      font-weight: 600;
      border-radius: 12px;
      cursor: pointer;
      display: inline-flex;
      align-items: center;
      justify-content: center;
      gap: 0.5rem;
      text-decoration: none;
      transition: all 0.2s ease;
      font-family: inherit;
    }
    .btn-download {
      background: transparent;
      border: 1px solid var(--border);
      color: var(--text);
    }
    .btn-download:hover {
      background: rgba(255, 255, 255, 0.05);
      border-color: rgba(255, 255, 255, 0.25);
    }
    
    .card.small .btn-api { background: var(--small-color); border: none; color: #022c22; }
    .card.small .btn-api:hover { background: #059669; }
    
    .card.medium .btn-api { background: var(--medium-color); border: none; color: #1e3a8a; }
    .card.medium .btn-api:hover { background: #2563eb; }
    
    .card.large .btn-api { background: var(--large-color); border: none; color: #4a044e; }
    .card.large .btn-api:hover { background: #c084fc; }

    .progress-container {
      background: rgba(0, 0, 0, 0.2);
      border-radius: 8px;
      padding: 1rem;
      display: none;
      border: 1px solid var(--border);
    }
    .progress-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      font-size: 0.8rem;
      margin-bottom: 0.5rem;
      font-weight: 600;
    }
    .progress-bar-bg {
      width: 100%;
      height: 6px;
      background: rgba(255, 255, 255, 0.05);
      border-radius: 9999px;
      overflow: hidden;
      margin-bottom: 0.5rem;
    }
    .progress-bar-fill {
      width: 0%;
      height: 100%;
      border-radius: 9999px;
      transition: width 0.1s ease;
    }
    .card.small .progress-bar-fill { background: var(--small-color); box-shadow: 0 0 10px var(--small-color); }
    .card.medium .progress-bar-fill { background: var(--medium-color); box-shadow: 0 0 10px var(--medium-color); }
    .card.large .progress-bar-fill { background: var(--large-color); box-shadow: 0 0 10px var(--large-color); }

    .progress-stats {
      display: flex;
      justify-content: space-between;
      font-size: 0.75rem;
      color: var(--text-muted);
      font-family: 'JetBrains Mono', monospace;
    }

    .terminal-container {
      width: 100%;
      max-width: 1100px;
      background: var(--terminal-bg);
      border: 1px solid var(--border);
      border-radius: 20px;
      overflow: hidden;
      box-shadow: 0 15px 35px rgba(0, 0, 0, 0.5);
    }
    .terminal-header {
      background: rgba(255, 255, 255, 0.03);
      padding: 0.75rem 1.5rem;
      display: flex;
      justify-content: space-between;
      align-items: center;
      border-bottom: 1px solid var(--border);
    }
    .terminal-title {
      font-family: 'JetBrains Mono', monospace;
      font-size: 0.85rem;
      font-weight: 600;
      color: var(--text-muted);
      display: flex;
      align-items: center;
      gap: 0.5rem;
    }
    .terminal-dots {
      display: flex;
      gap: 0.35rem;
    }
    .dot {
      width: 10px;
      height: 10px;
      border-radius: 50%;
    }
    .dot.red { background: #ef4444; }
    .dot.yellow { background: #f59e0b; }
    .dot.green { background: #10b981; }

    .terminal-body {
      padding: 1.5rem;
      font-family: 'JetBrains Mono', monospace;
      font-size: 0.85rem;
      color: #34d399; /* Green text */
      height: 250px;
      overflow-y: auto;
      line-height: 1.6;
      background: rgba(0, 0, 0, 0.3);
    }
    .log-line {
      margin-bottom: 0.4rem;
      word-break: break-all;
    }
    .log-time {
      color: #6b7280;
    }
    .log-info { color: #60a5fa; }
    .log-success { color: #34d399; }
    .log-error { color: #f87171; }
    .log-warning { color: #fbbf24; }

    .terminal-controls {
      display: flex;
      gap: 1rem;
    }
    .terminal-btn {
      background: transparent;
      border: 1px solid var(--border);
      color: var(--text-muted);
      padding: 0.25rem 0.75rem;
      border-radius: 6px;
      font-size: 0.75rem;
      font-family: inherit;
      cursor: pointer;
      transition: all 0.2s;
    }
    .terminal-btn:hover {
      color: var(--text);
      border-color: rgba(255, 255, 255, 0.2);
      background: rgba(255, 255, 255, 0.03);
    }

    .status-indicator {
      display: inline-flex;
      align-items: center;
      gap: 0.5rem;
      font-size: 0.85rem;
      font-weight: 600;
      padding: 0.35rem 0.85rem;
      border-radius: 9999px;
      background: rgba(255, 255, 255, 0.04);
      border: 1px solid var(--border);
      margin-bottom: 1rem;
    }
    .status-dot {
      width: 8px;
      height: 8px;
      border-radius: 50%;
      background: #9ca3af;
      transition: all 0.3s ease;
    }
    .status-dot.online {
      background: #10b981;
      box-shadow: 0 0 8px #10b981;
    }
    .status-dot.offline {
      background: #ef4444;
      box-shadow: 0 0 8px #ef4444;
    }

    footer {
      margin-top: auto;
      padding: 3rem 1rem 1rem 1rem;
      text-align: center;
      font-size: 0.85rem;
      color: var(--text-muted);
    }
  </style>
</head>
<body>

  <header>
    <div class="badge">Static File Server</div>
    <div class="status-indicator">
      <span class="status-dot" id="server-status-dot"></span>
      <span id="server-status-text">Checking connection...</span>
    </div>
    <h1>C++ Multi-Size File Downloader</h1>
    <p class="subtitle">A premium demonstration of static file serving, HTTP Range requests (partial content), and network throughput metrics using the <code>cpp-http</code> server library.</p>
  </header>

  <div class="grid">
    <!-- Small File Card -->
    <div class="card small" id="small-card">
      <div class="file-icon">
        <svg xmlns="http://www.w3.org/2000/svg" width="36" height="36" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M14.5 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V7.5L14.5 2z"/><polyline points="14 2 14 8 20 8"/><line x1="16" y1="13" x2="8" y2="13"/><line x1="16" y1="17" x2="8" y2="17"/><line x1="10" y1="9" x2="8" y2="9"/></svg>
      </div>
      <div class="file-name">small_file.txt</div>
      <div class="file-size">
        <span>10.0 KB</span>
        <span class="size-tag">TXT</span>
      </div>
      <div class="file-desc">A lightweight plain text document containing static details. Used for micro-benchmarks, basic download handshake validation, and testing compression efficiency.</div>
      
      <div class="button-group">
        <a href="/small_file.txt" download class="btn btn-download">
          <svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="7 10 12 15 17 10"/><line x1="12" y1="15" x2="12" y2="3"/></svg>
          Browser Download
        </a>
        <button onclick="downloadViaAPI('small_file.txt', 'small')" class="btn btn-api">
          <svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="22 12 16 12 14 15 10 15 8 12 2 12"/><path d="M5.45 5.11L2 12v6a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2v-6l-3.45-6.89A2 2 0 0 0 16.76 4H7.24a2 2 0 0 0-1.79 1.11z"/></svg>
          Speed Test (No Save)
        </button>
      </div>

      <div class="progress-container" id="small-progress-container">
        <div class="progress-header">
          <span id="small-status">Idle</span>
          <span id="small-percent">0%</span>
        </div>
        <div class="progress-bar-bg">
          <div class="progress-bar-fill" id="small-bar"></div>
        </div>
        <div class="progress-stats">
          <span id="small-loaded">0 B</span>
          <span id="small-speed">0 B/s</span>
        </div>
      </div>
    </div>

    <!-- Medium File Card -->
    <div class="card medium" id="medium-card">
      <div class="file-icon">
        <svg xmlns="http://www.w3.org/2000/svg" width="36" height="36" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"/><polyline points="14 2 14 8 20 8"/><line x1="16" y1="13" x2="8" y2="13"/><line x1="16" y1="17" x2="8" y2="17"/><polyline points="10 9 9 9 8 9"/></svg>
      </div>
      <div class="file-name">medium_file.bin</div>
      <div class="file-size">
        <span>5.00 MB</span>
        <span class="size-tag">BIN</span>
      </div>
      <div class="file-desc">A mid-size binary payload. Ideal for assessing network latency and throughput under regular network conditions and testing buffering speeds on client browsers.</div>
      
      <div class="button-group">
        <a href="/medium_file.bin" download class="btn btn-download">
          <svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="7 10 12 15 17 10"/><line x1="12" y1="15" x2="12" y2="3"/></svg>
          Browser Download
        </a>
        <button onclick="downloadViaAPI('medium_file.bin', 'medium')" class="btn btn-api">
          <svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="22 12 16 12 14 15 10 15 8 12 2 12"/><path d="M5.45 5.11L2 12v6a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2v-6l-3.45-6.89A2 2 0 0 0 16.76 4H7.24a2 2 0 0 0-1.79 1.11z"/></svg>
          Speed Test (No Save)
        </button>
      </div>

      <div class="progress-container" id="medium-progress-container">
        <div class="progress-header">
          <span id="medium-status">Idle</span>
          <span id="medium-percent">0%</span>
        </div>
        <div class="progress-bar-bg">
          <div class="progress-bar-fill" id="medium-bar"></div>
        </div>
        <div class="progress-stats">
          <span id="medium-loaded">0 B</span>
          <span id="medium-speed">0 B/s</span>
        </div>
      </div>
    </div>

    <!-- Large File Card -->
    <div class="card large" id="large-card">
      <div class="file-icon">
        <svg xmlns="http://www.w3.org/2000/svg" width="36" height="36" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M21 16V8a2 2 0 0 0-1-1.73l-7-4a2 2 0 0 0-2 0l-7 4A2 2 0 0 0 3 8v8a2 2 0 0 0 1 1.73l7 4a2 2 0 0 0 2 0l7-4A2 2 0 0 0 21 16z"/><polyline points="3.27 6.96 12 12.01 20.73 6.96"/><line x1="12" y1="22.08" x2="12" y2="12"/></svg>
      </div>
      <div class="file-name">large_file.bin</div>
      <div class="file-size">
        <span>25.00 MB</span>
        <span class="size-tag">BIN</span>
      </div>
      <div class="file-desc">A heavy binary container. Highly effective for highlighting memory efficiency, testing streaming chunk structures, measuring bandwidth speed curves, and observing browser range retrieval.</div>
      
      <div class="button-group">
        <a href="/large_file.bin" download class="btn btn-download">
          <svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="7 10 12 15 17 10"/><line x1="12" y1="15" x2="12" y2="3"/></svg>
          Browser Download
        </a>
        <button onclick="downloadViaAPI('large_file.bin', 'large')" class="btn btn-api">
          <svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="22 12 16 12 14 15 10 15 8 12 2 12"/><path d="M5.45 5.11L2 12v6a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2v-6l-3.45-6.89A2 2 0 0 0 16.76 4H7.24a2 2 0 0 0-1.79 1.11z"/></svg>
          Speed Test (No Save)
        </button>
      </div>

      <div class="progress-container" id="large-progress-container">
        <div class="progress-header">
          <span id="large-status">Idle</span>
          <span id="large-percent">0%</span>
        </div>
        <div class="progress-bar-bg">
          <div class="progress-bar-fill" id="large-bar"></div>
        </div>
        <div class="progress-stats">
          <span id="large-loaded">0 B</span>
          <span id="large-speed">0 B/s</span>
        </div>
      </div>
    </div>
  </div>

  <div class="terminal-container">
    <div class="terminal-header">
      <div class="terminal-title">
        <div class="terminal-dots">
          <div class="dot red"></div>
          <div class="dot yellow"></div>
          <div class="dot green"></div>
        </div>
        <span>C++ Server Logs & Diagnostic Console</span>
      </div>
      <div class="terminal-controls">
        <button class="terminal-btn" onclick="clearLogs()">Clear Console</button>
      </div>
    </div>
    <div class="terminal-body" id="log-area">
      <!-- Logs populated dynamically at runtime -->
    </div>
  </div>

  <footer>
    <p>Powered by <strong>cpp-http</strong>. Responsive layout styled using Premium CSS.</p>
  </footer>

  <script>
    function clearLogs() {
      const logArea = document.getElementById('log-area');
      logArea.innerHTML = '<div class="log-line"><span class="log-time">[' + new Date().toLocaleTimeString() + ']</span> <span class="log-info">Console cleared.</span></div>';
    }

    function formatBytes(bytes) {
      if (bytes === 0) return '0 Bytes';
      const k = 1024;
      const sizes = ['Bytes', 'KB', 'MB', 'GB'];
      const i = Math.floor(Math.log(bytes) / Math.log(k));
      return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
    }

    function formatSpeed(bytesPerSec) {
      if (bytesPerSec >= 1024 * 1024) {
        return (bytesPerSec / (1024 * 1024)).toFixed(2) + ' MB/s';
      } else if (bytesPerSec >= 1024) {
        return (bytesPerSec / 1024).toFixed(1) + ' KB/s';
      }
      return bytesPerSec.toFixed(0) + ' B/s';
    }

    const logArea = document.getElementById('log-area');
    const logMessage = (msg, type = 'info') => {
      const timestamp = new Date().toLocaleTimeString();
      const line = document.createElement('div');
      line.className = 'log-line';
      let colorClass = 'log-info';
      if (type === 'success') colorClass = 'log-success';
      if (type === 'error') colorClass = 'log-error';
      if (type === 'warning') colorClass = 'log-warning';
      
      line.innerHTML = `<span class="log-time">[${timestamp}]</span> <span class="${colorClass}">${msg}</span>`;
      logArea.appendChild(line);
      logArea.scrollTop = logArea.scrollHeight;
    };

    // Populate initial logs with actual runtime timestamps
    logMessage("Diagnostics initialized. Ready for connections.", "info");
    logMessage("StaticDir root mapped to local workspace sandbox.", "success");

    let serverOnline = true;
    async function checkServerStatus() {
      const dot = document.getElementById('server-status-dot');
      const text = document.getElementById('server-status-text');
      try {
        const controller = new AbortController();
        const timeoutId = setTimeout(() => controller.abort(), 1500);
        const response = await fetch('/?t=' + Date.now(), {
          method: 'HEAD',
          signal: controller.signal
        });
        clearTimeout(timeoutId);

        if (response.ok || response.status < 500) {
          if (!serverOnline) {
            serverOnline = true;
            logMessage("Server connection re-established. Ready for requests.", "success");
          }
          dot.className = 'status-dot online';
          text.textContent = 'Server Online';
          text.style.color = '#10b981';
        }
      } catch (err) {
        if (serverOnline) {
          serverOnline = false;
          logMessage("Connection to server lost. Speed tests will fail until restarted.", "error");
        }
        dot.className = 'status-dot offline';
        text.textContent = 'Server Offline';
        text.style.color = '#ef4444';
      }
    }

    checkServerStatus();
    setInterval(checkServerStatus, 3000);

    async function downloadViaAPI(fileName, cardId) {
      const statusText = document.getElementById(`${cardId}-status`);
      if (!serverOnline) {
        logMessage(`Speed test aborted: Server is offline. Please start the server and try again.`, 'error');
        const container = document.getElementById(`${cardId}-progress-container`);
        container.style.display = 'block';
        statusText.textContent = 'Offline';
        statusText.style.color = '#ef4444';
        return;
      }

      const container = document.getElementById(`${cardId}-progress-container`);
      const progressBar = document.getElementById(`${cardId}-bar`);
      const percentText = document.getElementById(`${cardId}-percent`);
      const loadedText = document.getElementById(`${cardId}-loaded`);
      const speedText = document.getElementById(`${cardId}-speed`);
      
      container.style.display = 'block';
      progressBar.style.width = '0%';
      percentText.textContent = '0%';
      loadedText.textContent = '0 B';
      speedText.textContent = '0 B/s';
      statusText.textContent = 'Connecting';
      statusText.style.color = '#fbbf24';

      logMessage(`Initiating API speed test: ${fileName} (Chunks are discarded in-memory, no file is saved)`, 'warning');
      const startTime = performance.now();
      let lastTime = startTime;
      let loaded = 0;
      let lastLoaded = 0;

      try {
        const response = await fetch(`/${fileName}`);
        if (!response.ok) {
          throw new Error(`HTTP Error status: ${response.status} ${response.statusText}`);
        }

        statusText.textContent = 'Downloading';
        statusText.style.color = '#38bdf8';
        
        const contentLength = parseInt(response.headers.get('Content-Length'), 10);
        if (isNaN(contentLength)) {
          logMessage(`Stream size unknown. Download starts...`, 'warning');
        } else {
          logMessage(`Streaming file: ${fileName} (${formatBytes(contentLength)})`, 'info');
        }

        const reader = response.body.getReader();
        while (true) {
          const { done, value } = await reader.read();
          if (done) break;

          loaded += value.length;
          const now = performance.now();
          const elapsed = now - lastTime;

          if (elapsed >= 100) {
            const currentSpeed = (loaded - lastLoaded) / (elapsed / 1000);
            speedText.textContent = formatSpeed(currentSpeed);
            loadedText.textContent = formatBytes(loaded);

            if (!isNaN(contentLength)) {
              const percent = Math.min(100, (loaded / contentLength) * 100);
              percentText.textContent = percent.toFixed(1) + '%';
              progressBar.style.width = percent + '%';
            } else {
              percentText.textContent = 'Streaming';
              progressBar.style.width = '100%';
            }

            lastTime = now;
            lastLoaded = loaded;
          }
        }

        const totalTime = (performance.now() - startTime) / 1000;
        const avgSpeed = loaded / totalTime;
        
        progressBar.style.width = '100%';
        percentText.textContent = '100%';
        loadedText.textContent = formatBytes(loaded);
        speedText.textContent = `Avg: ${formatSpeed(avgSpeed)}`;
        statusText.textContent = 'Completed';
        statusText.style.color = '#10b981';

        logMessage(`Download completed: ${fileName} (${formatBytes(loaded)} in ${totalTime.toFixed(2)}s at ${formatSpeed(avgSpeed)})`, 'success');

      } catch (err) {
        statusText.textContent = 'Failed';
        statusText.style.color = '#ef4444';
        logMessage(`Transfer error: ${err.message}`, 'error');
      }
    }
  </script>
</body>
</html>
)html";

// Helper to construct a file filled with a specific character/pattern
void create_dummy_file(const std::string &filepath, size_t size_in_bytes,
                       char fill_char) {
  std::ofstream out(filepath, std::ios::binary);
  if (!out) {
    std::cerr << "Failed to create file: " << filepath << std::endl;
    return;
  }
  const size_t chunk_size = 65536;
  std::vector<char> buffer(chunk_size, fill_char);
  size_t bytes_written = 0;
  while (bytes_written < size_in_bytes) {
    size_t to_write = std::min(chunk_size, size_in_bytes - bytes_written);
    out.write(buffer.data(), to_write);
    bytes_written += to_write;
  }
}

// Helper to construct small text file with readable contents
void create_small_text_file(const std::string &filepath) {
  std::ofstream out(filepath);
  if (!out) {
    std::cerr << "Failed to create text file: " << filepath << std::endl;
    return;
  }
  std::string pattern =
      "cpp-http library static file serving demo text. This is a small text "
      "file designed to show fast download capabilities. ";
  size_t bytes_written = 0;
  while (bytes_written < 10 * 1024) {
    out << pattern << "\n";
    bytes_written += pattern.size() + 1;
  }
}

int main() {
  std::string demo_dir = "./demo_download_dist";
  std::error_code ec;
  std::filesystem::create_directories(demo_dir, ec);

  // 1. Create index.html
  {
    std::ofstream out(demo_dir + "/index.html");
    if (!out) {
      std::cerr << "Failed to write index.html to " << demo_dir << std::endl;
      return 1;
    }
    out << UI_HTML;
  }

  // 2. Generate small text file (10 KB)
  create_small_text_file(demo_dir + "/small_file.txt");

  // 3. Generate medium binary file (5 MB)
  create_dummy_file(demo_dir + "/medium_file.bin", 5 * 1024 * 1024, 'B');

  // 4. Generate large binary file (25 MB)
  create_dummy_file(demo_dir + "/large_file.bin", 25 * 1024 * 1024, 'C');

  uint16_t port = 8080;
  cpphttp::HttpServer server(port);

  // Serve static files from root directory mapping to demo_dir
  server.StaticDir("/", demo_dir, false);

  std::cout << "========================================================\n";
  std::cout << "⚡ C++ Static File Download Demo Server Started\n";
  std::cout << "========================================================\n";
  std::cout << "Serving files:\n";
  std::cout << " - Root UI:          http://localhost:" << port << "/\n";
  std::cout << " - Small File (txt): http://localhost:" << port
            << "/small_file.txt\n";
  std::cout << " - Medium File (bin):http://localhost:" << port
            << "/medium_file.bin\n";
  std::cout << " - Large File (bin): http://localhost:" << port
            << "/large_file.bin\n";
  std::cout << "========================================================\n";
  std::cout << "Press Enter to stop the server...\n";

  try {
    server.Start();
    std::cin.get();
    server.Stop();
  } catch (const std::exception &e) {
    std::cerr << "Server Error: " << e.what() << std::endl;
  }

  // Cleanup files and directory
  std::filesystem::remove_all(demo_dir, ec);
  std::cout << "Server stopped and cleanup complete.\n";

  return 0;
}
