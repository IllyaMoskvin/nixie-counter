import nixie
import sys

class ReadFileHTTPRequestHandler(nixie.NixieHTTPRequestHandler):

    def get_data(self):
        with open(sys.argv[1], 'r') as file:
            return int(file.read().replace('\n', ''))

nixie.serve(ReadFileHTTPRequestHandler)
