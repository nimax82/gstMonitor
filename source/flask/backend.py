from flask import Flask, render_template, send_from_directory, request
import os

app = Flask(__name__, template_folder='../../http')
app.debug = True

# Directory where video files are stored
# TODO: ABSOLUTE PATH IN A DOCKER
VIDEO_DIR = '../../resources/video/static'
HLS_DIR =  '../../resources/video/hls'  # Directory where HLS files are stored '/var/www/html/hls'

@app.route('/')
def index():
    # List all video files in the directory
    files = sorted([f for f in os.listdir(VIDEO_DIR) if f.endswith('.mp4') and not f.startswith('.')])
    return render_template('index_static.html', files=files)

@app.route('/video/<filename>')
def video(filename):
    # Serve the selected video file
    return send_from_directory(VIDEO_DIR, filename)

@app.route('/hls/<path:filename>')
def hls(filename):
    # Serve the HLS files
    print("DEBUGG: " + str(send_from_directory(HLS_DIR, filename)))
    return send_from_directory(HLS_DIR, filename)

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)