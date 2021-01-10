import nixie
import sys
import os

class FileCountHTTPRequestHandler(nixie.NixieHTTPRequestHandler):

    def __init__(self, *args, **kwargs):
        if len(sys.argv) > 1 and os.path.isdir(sys.argv[1]):
            os.chdir(sys.argv[1])

        super().__init__(*args, **kwargs)

    def get_data(self):
        # counts both files and folders, for speed
        # https://stackoverflow.com/questions/2632205
        # surprisingly, faster than `ls -f | wc -l`
        return len(os.listdir('.'))

nixie.serve(FileCountHTTPRequestHandler)
