# __init__.py
from http.server import BaseHTTPRequestHandler, HTTPServer
import json
import sys
import time

class ProviderNameMockServer(BaseHTTPRequestHandler):
    def _set_headers(self, status_code=200):
        self.send_response(status_code)
        self.send_header('Content-type', 'application/json')
        self.end_headers()

    def do_POST(self):
        content_length = int(self.headers['Content-Length'])
        post_data = self.rfile.read(content_length)
        data = json.loads(post_data.decode('utf-8'))

        if self.path == '/job':
            # Create a job
            response = {
                'uuid': 'job-123',
                'status': 'WAITING'
            }
            self._set_headers()
            self.wfile.write(json.dumps(response).encode())
        else:
            self._set_headers(404)
            self.wfile.write(json.dumps({'error': 'Not found'}).encode())

    def do_GET(self):
        if self.path.startswith('/job/job-123'):
            # Return job status and results
            response = {
                "uuid": "job-123",
                "status": "COMPLETED",
                "result": json.dumps({
                  "00": 500,
                  "11": 500
                })
            }
            self._set_headers()
            self.wfile.write(json.dumps(response).encode())
        else:
            self._set_headers(404)
            self.wfile.write(json.dumps({'error': 'Not found'}).encode())

def startServer(port=8000):
    server_address = ('', port)
    httpd = HTTPServer(server_address, ProviderNameMockServer)
    print(f'Starting mock server on port {port}...')
    httpd.serve_forever()

if __name__ == '__main__':
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8000
    startServer(port)

