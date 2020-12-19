import nixie
import random

class RandomHTTPRequestHandler(nixie.NixieHTTPRequestHandler):

    def get_data(self):
        return random.randint(0,999999)

nixie.serve(RandomHTTPRequestHandler)
