/**
 * @file server.js
 * @brief High-Performance HTTP & Server-Sent Events (SSE) Server
 * @course Operating Systems and Systems Programming (25CS2104E) - Section 10, Team 10
 * 
 * SERVER ARCHITECTURE & RESPONSIBILITIES:
 * -----------------------------------------
 * 1. ZERO-DEPENDENCY HTTP SERVER:
 *    Built strictly with native Node.js core modules (`http`, `fs`, `path`, `child_process`).
 *    Requires no third-party npm packages, ensuring 100% portable 1-click execution.
 * 
 * 2. DUAL SIMULATION MODES:
 *    - Interactive Visual Simulation: Served as static HTML/CSS/JS running entirely in browser.
 *    - Native Linux POSIX C Binary: Spawns the compiled GCC binary in WSL (`wsl bash -c`),
 *      streaming its standard output line-by-line via Server-Sent Events (SSE) to the browser.
 * 
 * 3. REST API ENDPOINTS:
 *    - GET  /api/info           : Returns project course metadata, roll numbers, and responsibilities.
 *    - GET  /api/benchmark-data : Returns precomputed multi-scenario stress benchmark results.
 *    - GET  /api/run-c-stream   : SSE stream executing Linux C binary in WSL with live logs.
 *    - POST /api/run-benchmark  : Executes the C benchmark stress matrix and updates benchmark JSON.
 */

const http = require('http');
const fs = require('fs');
const path = require('path');
const { spawn, exec } = require('child_process');

/* Server Configuration */
const PORT = process.env.PORT || 3000;
const ROOT_DIR = path.resolve(__dirname, '..');
const FRONTEND_DIR = path.join(ROOT_DIR, 'frontend');
const C_CORE_DIR = path.join(ROOT_DIR, 'c-core');
const BENCHMARK_JSON_PATH = path.join(C_CORE_DIR, 'benchmark_results.json');

/* Standard MIME Types for Static File Serving */
const MIME_TYPES = {
    '.html': 'text/html; charset=utf-8',
    '.css':  'text/css; charset=utf-8',
    '.js':   'text/javascript; charset=utf-8',
    '.json': 'application/json; charset=utf-8',
    '.png':  'image/png',
    '.jpg':  'image/jpeg',
    '.svg':  'image/svg+xml',
    '.ico':  'image/x-icon'
};

/* Official Academic Project Metadata */
const PROJECT_METADATA = {
    course: "Operating Systems and Systems Programming (25CS2104E)",
    term: "2026-27, Term-I",
    institution: "Koneru Lakshmaiah Education Foundation (KLH)",
    campus: "Off-Campus: Bachupally-Gandimaisamma Road, Bowrampet, Hyderabad, Telangana",
    section: 10,
    team: 10,
    title: "Concurrent Producer-Consumer System Using Semaphores",
    team_members: [
        {
            roll: "2520090085",
            name: "Vakkalanka Sai Bhaskara Sujit",
            responsibility: "Core circular buffer implementation, memory allocation, data structures design, concurrency testing, and edge-case validation."
        },
        {
            roll: "2520090087",
            name: "Duddukuri Bala Charan",
            responsibility: "Semaphore logic implementation (sem_init, sem_wait, sem_post) and mutex locking. Performance profiling and throughput benchmarking."
        },
        {
            roll: "2520090023",
            name: "Matta Sai Chaitanya",
            responsibility: "Thread creation, lifecycle management using POSIX pthreads, and thread worker routines."
        }
    ],
    github: "https://github.com/mattasaichaitanya-gif/KLH-CS-IT_2026-27_Batch10_LinuxProject"
};

function sendJSON(res, statusCode, data) {
    res.writeHead(statusCode, {
        'Content-Type': 'application/json; charset=utf-8',
        'Access-Control-Allow-Origin': '*',
        'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
        'Access-Control-Allow-Headers': 'Content-Type'
    });
    res.end(JSON.stringify(data, null, 2));
}

function parseRequestBody(req) {
    return new Promise((resolve, reject) => {
        let body = '';
        req.on('data', chunk => { body += chunk; });
        req.on('end', () => {
            try {
                resolve(body ? JSON.parse(body) : {});
            } catch (err) {
                resolve({});
            }
        });
        req.on('error', reject);
    });
}

function serveStatic(req, res, filePath) {
    fs.readFile(filePath, (err, content) => {
        if (err) {
            if (err.code === 'ENOENT') {
                res.writeHead(404, { 'Content-Type': 'text/plain' });
                res.end('404 Not Found');
            } else {
                res.writeHead(500, { 'Content-Type': 'text/plain' });
                res.end(`Server Error: ${err.code}`);
            }
        } else {
            const ext = path.extname(filePath).toLowerCase();
            const contentType = MIME_TYPES[ext] || 'application/octet-stream';
            res.writeHead(200, { 'Content-Type': contentType });
            res.end(content);
        }
    });
}

const server = http.createServer(async (req, res) => {
    // Handle CORS preflight
    if (req.method === 'OPTIONS') {
        res.writeHead(204, {
            'Access-Control-Allow-Origin': '*',
            'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
            'Access-Control-Allow-Headers': 'Content-Type'
        });
        res.end();
        return;
    }

    const url = new URL(req.url, `http://${req.headers.host}`);
    const pathname = url.pathname;

    // API: Project Info
    if (pathname === '/api/info' && req.method === 'GET') {
        return sendJSON(res, 200, PROJECT_METADATA);
    }

    // API: Get Stored Benchmark Results
    if (pathname === '/api/benchmark-data' && req.method === 'GET') {
        if (fs.existsSync(BENCHMARK_JSON_PATH)) {
            try {
                const data = JSON.parse(fs.readFileSync(BENCHMARK_JSON_PATH, 'utf-8'));
                return sendJSON(res, 200, data);
            } catch (e) {
                return sendJSON(res, 500, { error: "Failed to parse benchmark file" });
            }
        } else {
            return sendJSON(res, 404, { error: "Benchmark data not generated yet. Run benchmark first." });
        }
    }

    function getWSLPath(windowsPath) {
        const resolved = path.resolve(windowsPath).replace(/\\/g, '/');
        return resolved.replace(/^([a-zA-Z]):/, (match, drive) => `/mnt/${drive.toLowerCase()}`);
    }

    // API: Run Live C Binary in Linux/WSL and Stream Output via SSE
    if (pathname === '/api/run-c-stream' && req.method === 'GET') {
        const bufferSize = parseInt(url.searchParams.get('b') || '5', 10);
        const producers = parseInt(url.searchParams.get('p') || '2', 10);
        const consumers = parseInt(url.searchParams.get('c') || '2', 10);
        const items = parseInt(url.searchParams.get('n') || '20', 10);
        const pDelay = parseInt(url.searchParams.get('pdelay') || '50', 10);
        const cDelay = parseInt(url.searchParams.get('cdelay') || '75', 10);

        res.writeHead(200, {
            'Content-Type': 'text/event-stream',
            'Cache-Control': 'no-cache',
            'Connection': 'keep-alive',
            'Access-Control-Allow-Origin': '*'
        });

        res.write(`data: ${JSON.stringify({ type: 'start', message: 'Launching native Linux POSIX C binary in WSL...' })}\n\n`);

        const wslDir = getWSLPath(C_CORE_DIR);
        const cmd = `cd '${wslDir}' && ./bin/producer_consumer -b ${bufferSize} -p ${producers} -c ${consumers} -n ${items} --p-delay ${pDelay} --c-delay ${cDelay}`;

        let child;
        try {
            child = spawn('wsl', ['bash', '-c', cmd]);
        } catch (err) {
            res.write(`data: ${JSON.stringify({ type: 'error', text: 'Failed to spawn WSL process: ' + err.message })}\n\n`);
            res.write(`data: ${JSON.stringify({ type: 'done', exitCode: 1 })}\n\n`);
            res.end();
            return;
        }

        child.on('error', err => {
            res.write(`data: ${JSON.stringify({ type: 'error', text: 'WSL execution error: ' + err.message + '. Tip: Use the Visual Simulation mode for instant in-browser execution.' })}\n\n`);
            res.write(`data: ${JSON.stringify({ type: 'done', exitCode: 1 })}\n\n`);
            res.end();
        });

        child.stdout.on('data', chunk => {
            const lines = chunk.toString().split('\n');
            for (const line of lines) {
                if (line.trim().length > 0) {
                    res.write(`data: ${JSON.stringify({ type: 'log', text: line })}\n\n`);
                }
            }
        });

        child.stderr.on('data', chunk => {
            res.write(`data: ${JSON.stringify({ type: 'error', text: chunk.toString() })}\n\n`);
        });

        child.on('close', code => {
            res.write(`data: ${JSON.stringify({ type: 'done', exitCode: code })}\n\n`);
            res.end();
        });

        req.on('close', () => {
            child.kill();
        });
        return;
    }

    // API: Run Benchmark Suite
    if (pathname === '/api/run-benchmark' && req.method === 'POST') {
        const wslDir = getWSLPath(C_CORE_DIR);
        const cmd = `cd '${wslDir}' && ./bin/producer_consumer -B -j benchmark_results.json`;
        exec(`wsl bash -c "${cmd}"`, (err, stdout, stderr) => {
            if (err) {
                return sendJSON(res, 500, { error: err.message, stderr, stdout });
            }
            try {
                if (fs.existsSync(BENCHMARK_JSON_PATH)) {
                    const data = JSON.parse(fs.readFileSync(BENCHMARK_JSON_PATH, 'utf-8'));
                    return sendJSON(res, 200, { success: true, data, stdout });
                }
                return sendJSON(res, 200, { success: true, stdout });
            } catch (e) {
                return sendJSON(res, 500, { error: "Failed to read benchmark output" });
            }
        });
        return;
    }

    // Static Frontend Routing
    let safePath = pathname === '/' ? '/index.html' : pathname;
    let targetFile = path.join(FRONTEND_DIR, safePath);

    // Security check to avoid path traversal
    if (!targetFile.startsWith(FRONTEND_DIR)) {
        res.writeHead(403, { 'Content-Type': 'text/plain' });
        res.end('Access Denied');
        return;
    }

    serveStatic(req, res, targetFile);
});

server.listen(PORT, () => {
    console.log(`================================================================`);
    console.log(`  Producer-Consumer System Web Visualizer & Server`);
    console.log(`  Course: OS & Systems Programming (25CS2104E) - Team 10`);
    console.log(`  Live URL: http://localhost:${PORT}`);
    console.log(`================================================================`);
});
