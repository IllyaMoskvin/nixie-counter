from http.server import HTTPServer, BaseHTTPRequestHandler

def serve(handler):
    # https://www.speedguide.net/port.php?port=4224
    httpd = HTTPServer(('', 4224), handler)
    httpd.serve_forever()

class NixieHTTPRequestHandler(BaseHTTPRequestHandler):

    protocol_version = 'HTTP/1.1'

    # Override this method in child class
    def get_data(self):
        return '123456'

    def do_GET(self):
        body = self.get_data()

        # End body with trailing \n to avoid 5 second delay:
        body = str(body) + '\n'

        self.send_response(200)
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()

        self.wfile.write(str.encode(body))
