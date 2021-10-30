import nixie
import sys

class ReadFileHTTPRequestHandler(nixie.NixieHTTPRequestHandler):

    def get_data(self):
        path = ' '.join(sys.argv[1:])
        with open(path, 'r') as file:
            return int(file.read().replace('\n', ''))

nixie.serve(ReadFileHTTPRequestHandler)
