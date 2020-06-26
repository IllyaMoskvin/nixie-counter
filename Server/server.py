from http.server import HTTPServer, BaseHTTPRequestHandler
import requests

class SimpleHTTPRequestHandler(BaseHTTPRequestHandler):

    protocol_version = 'HTTP/1.1'

    def do_GET(self):

        response = requests.get('https://nocache.aggregator-data.artic.edu/api/v1/artworks/search', {
            # Bypass an internal Elasticsearch cache (use only for realtime applications)
            'cache': 'false',
            # We only care about `pagination`, not `data`
            'limit': 0,
            # All artworks updated since 9:00 AM today
            'query[range][timestamp][gte]': 'now/1d+9h',
            # ...relative to local time in Chicago, IL
            'query[range][timestamp][time_zone]': '-06:00',
        })

        data = response.json()

        body = data['pagination']['total']

        body = str(body) + '\n'

        self.send_response(200)
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()

        self.wfile.write(str.encode(body))

# https://www.speedguide.net/port.php?port=4224
httpd = HTTPServer(('', 4224), SimpleHTTPRequestHandler)

httpd.serve_forever()
