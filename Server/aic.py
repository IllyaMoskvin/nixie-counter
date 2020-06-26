import nixie
import requests

class AICHTTPRequestHandler(nixie.NixieHTTPRequestHandler):
    def get_data(self):
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

        return data['pagination']['total']

nixie.serve(AICHTTPRequestHandler)
