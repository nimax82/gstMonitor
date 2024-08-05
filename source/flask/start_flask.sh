#!/bin/bash

source ../../.flaskvenv/bin/activate

export FLASK_APP=backend.py

flask run --host=0.0.0.0
