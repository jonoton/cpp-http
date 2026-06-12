#include "cpphttp.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>

int main() {
  // Create a dummy SPA directory structure for the example
  std::string spa_dir = "./demo_spa_dist";
  std::filesystem::create_directories(spa_dir);
  std::filesystem::create_directories(spa_dir + "/static/css");

  {
    std::ofstream out(spa_dir + "/index.html");
    out << "<!DOCTYPE html>\n"
        << "<html lang=\"en\">\n"
        << "<head>\n"
        << "  <meta charset=\"UTF-8\">\n"
        << "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
        << "  <title>Modern SPA Demo</title>\n"
        << "  <link href=\"https://fonts.googleapis.com/css2?family=Inter:wght@400;600;800&display=swap\" rel=\"stylesheet\">\n"
        << "  <link rel=\"stylesheet\" href=\"/app/static/css/style.css\">\n"
        << "</head>\n"
        << "<body>\n"
        << "  <div class=\"container\">\n"
        << "    <header class=\"header\">\n"
        << "      <div class=\"logo\">🚀 SPA Demo</div>\n"
        << "      <nav class=\"nav\">\n"
        << "        <a href=\"/app/\" class=\"nav-link\">Home</a>\n"
        << "        <a href=\"/app/dashboard\" class=\"nav-link\">Dashboard</a>\n"
        << "        <a href=\"/app/users/123\" class=\"nav-link\">User Profile</a>\n"
        << "      </nav>\n"
        << "    </header>\n"
        << "    \n"
        << "    <main class=\"main-content\" id=\"app-content\">\n"
        << "      <!-- Content injected by JS router -->\n"
        << "    </main>\n"
        << "  </div>\n"
        << "  <script>\n"
        << "    const routes = {\n"
        << "      '/app/': {\n"
        << "        title: 'Welcome to the Single Page App',\n"
        << "        body: 'This is the Home page. Try clicking the navigation links above. You\\'ll notice the browser URL changes, but the server handles the fallback routing seamlessly!'\n"
        << "      },\n"
        << "      '/app': {\n"
        << "        title: 'Welcome to the Single Page App',\n"
        << "        body: 'This is the Home page. Try clicking the navigation links above. You\\'ll notice the browser URL changes, but the server handles the fallback routing seamlessly!'\n"
        << "      },\n"
        << "      '/app/dashboard': {\n"
        << "        title: 'Dashboard View',\n"
        << "        body: 'Here is your dashboard. This content was rendered entirely on the client side without a page reload.'\n"
        << "      },\n"
        << "      '/app/users/123': {\n"
        << "        title: 'User Profile: 123',\n"
        << "        body: 'Hello User 123! The server didn\\'t have a specific file for this route, but it served index.html and our client-side router took over.'\n"
        << "      }\n"
        << "    };\n"
        << "\n"
        << "    function render() {\n"
        << "      const path = window.location.pathname;\n"
        << "      const route = routes[path] || {\n"
        << "        title: '404 - Not Found',\n"
        << "        body: 'This route does not exist in our client-side router.'\n"
        << "      };\n"
        << "\n"
        << "      document.getElementById('app-content').innerHTML = `\n"
        << "        <h1 class=\"title\">${route.title}</h1>\n"
        << "        <p class=\"subtitle\">${route.body}</p>\n"
        << "        <div class=\"card\">\n"
        << "          <h3>Client-Side Routing Powered</h3>\n"
        << "          <p>This page is served from <code>index.html</code>. No matter what path you visit under <code>/app/*</code>, the C++ server will fallback to this file. Try refreshing the page on an unknown route!</p>\n"
        << "        </div>\n"
        << "      `;\n"
        << "    }\n"
        << "\n"
        << "    document.addEventListener('click', e => {\n"
        << "      if (e.target.matches('.nav-link')) {\n"
        << "        e.preventDefault();\n"
        << "        window.history.pushState(null, '', e.target.getAttribute('href'));\n"
        << "        render();\n"
        << "      }\n"
        << "    });\n"
        << "\n"
        << "    window.addEventListener('popstate', render);\n"
        << "    render();\n"
        << "  </script>\n"
        << "</body>\n"
        << "</html>\n";
  }
  {
    std::ofstream out(spa_dir + "/static/css/style.css");
    out << "body { font-family: 'Inter', sans-serif; background: #0f172a; color: #f8fafc; margin: 0; padding: 0; display: flex; justify-content: center; min-height: 100vh; }\n"
        << ".container { width: 100%; max-width: 800px; padding: 2rem; }\n"
        << ".header { display: flex; justify-content: space-between; align-items: center; padding-bottom: 2rem; border-bottom: 1px solid #334155; margin-bottom: 2rem; }\n"
        << ".logo { font-size: 1.5rem; font-weight: 800; background: linear-gradient(to right, #38bdf8, #818cf8); -webkit-background-clip: text; -webkit-text-fill-color: transparent; }\n"
        << ".nav { display: flex; gap: 1.5rem; }\n"
        << ".nav-link { color: #cbd5e1; text-decoration: none; font-weight: 600; transition: color 0.2s; }\n"
        << ".nav-link:hover { color: #38bdf8; }\n"
        << ".title { font-size: 3rem; margin-bottom: 1rem; line-height: 1.2; }\n"
        << ".subtitle { font-size: 1.1rem; color: #94a3b8; margin-bottom: 3rem; line-height: 1.6; }\n"
        << ".card { background: #1e293b; padding: 2rem; border-radius: 12px; box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1), 0 2px 4px -1px rgba(0, 0, 0, 0.06); border: 1px solid #334155; }\n"
        << ".card h3 { margin-top: 0; font-size: 1.5rem; color: #f1f5f9; }\n"
        << ".card p { color: #cbd5e1; line-height: 1.6; margin-bottom: 1.5rem; }\n"
        << "code { background: #0f172a; padding: 0.2rem 0.4rem; border-radius: 4px; color: #38bdf8; font-family: monospace; }\n";
  }

  // Run the HTTP server
  uint16_t port = 8080;
  cpphttp::HttpServer server(port);

  // Serve static files with SPA mode enabled
  // 1st arg: The route prefix to serve from
  // 2nd arg: The directory path on the file system
  // 3rd arg: spa_mode = true (fallback to index.html for 404s)
  server.StaticDir("/app", spa_dir, true);

  std::cout << "Starting SPA example server on port " << port << "...\n";
  std::cout << "Visit http://localhost:" << port << "/app/ in your browser.\n";
  
  server.Start();

  // Keep the main thread alive while the background server thread runs
  std::cout << "Press Enter to exit..." << std::endl;
  std::cin.get();

  server.Stop();
  
  // Cleanup
  std::filesystem::remove_all(spa_dir);

  return 0;
}
