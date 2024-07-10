from flask import Flask, jsonify
import os

app = Flask(__name__)

VIDEO_DIR = '/var/www/videos'

@app.route('/api/list_videos')
def list_videos():
    videos = []
    for filename in os.listdir(VIDEO_DIR):
        print("filename: ", filename)
        if filename.endswith('.avi'):
            videos.append(filename)
    return jsonify({'videos': videos})

if __name__ == '__main__':
    app.run(debug=True)
