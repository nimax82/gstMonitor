from flask import Flask, render_template, send_from_directory, request
import os

app = Flask(__name__, template_folder='http')

# Directory where video files are stored
# TODO: ABSOLUTE PATH IN A DOCKER
VIDEO_DIR = 'resources/video/static'

@app.route('/')
def index():
    # List all video files in the directory
    files = [f for f in os.listdir(VIDEO_DIR) if f.endswith('.mp4') and not f.startswith('.')]
    return render_template('index_static.html', files=files)

@app.route('/video/<filename>')
def video(filename):
    # Serve the selected video file
    return send_from_directory(VIDEO_DIR, filename)

if __name__ == '__main__':
    app.run(debug=True)