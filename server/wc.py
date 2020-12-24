import nixie
import sys
import re

class WordCountHTTPRequestHandler(nixie.NixieHTTPRequestHandler):

    file_path = '../README.md'

    def __init__(self, *args, **kwargs):
        if len(sys.argv) > 1:
            self.file_path = sys.argv[1]

        super().__init__(*args, **kwargs)

    def get_data(self):
        with open(self.file_path, 'r') as myfile:
            data = myfile.read()

        return self.count_words_in_markdown(data)

    # https://github.com/gandreadis/markdown-word-count/blob/master/mwc/counter.py
    def count_words_in_markdown(self, markdown):
        text = markdown

        # Comments
        text = re.sub(r'<!--(.*?)-->', '', text, flags=re.MULTILINE)
        # Tabs to spaces
        text = text.replace('\t', '    ')
        # More than 1 space to 4 spaces
        text = re.sub(r'[ ]{2,}', '    ', text)
        # Footnotes
        text = re.sub(r'^\[[^]]*\][^(].*', '', text, flags=re.MULTILINE)
        # Indented blocks of code
        text = re.sub(r'^( {4,}[^-*]).*', '', text, flags=re.MULTILINE)
        # Custom header IDs
        text = re.sub(r'{#.*}', '', text)
        # Replace newlines with spaces for uniform handling
        text = text.replace('\n', ' ')
        # Remove images
        text = re.sub(r'!\[[^\]]*\]\([^)]*\)', '', text)
        # Remove HTML tags
        text = re.sub(r'</?[^>]*>', '', text)
        # Remove special characters
        text = re.sub(r'[#*`~\-–^=<>+|/:]', '', text)
        # Remove footnote references
        text = re.sub(r'\[[0-9]*\]', '', text)
        # Remove enumerations
        text = re.sub(r'[0-9#]*\.', '', text)

        return len(text.split())

nixie.serve(WordCountHTTPRequestHandler)
