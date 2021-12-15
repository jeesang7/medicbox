# -*- coding:utf-8 -*-
import os
import sys
import json

# from flask import Flask, Response, request, jsonify
import flask
import time
import pathlib

webApp = flask.Flask(__name__)


@webApp.route("/data", methods=["GET"])
def index():
    acc_x = ''
    acc_y = ''
    acc_z = ''
    motion_pose = ''
    #Category = flask.request.args.get("Category")

    current_path = os.path.dirname(os.path.abspath(__file__))
    if (pathlib.Path(current_path + '/saved_data.json')).exists():
        with open(current_path + '/saved_data.json', 'r', encoding='utf8') as cf:
            data = json.load(cf)
            acc_x = data.get('Accel_X', '')
            acc_y = data.get('Accel_Y', '')
            acc_z = data.get('Accel_Z', '')
            motion_pose = data.get('Motion_Pose', '')

    return flask.jsonify({'acc_x': acc_x, 'acc_y': acc_y, 'acc_z': acc_z, 'motion_pose': motion_pose})


if __name__ == '__main__':
    allowed_external = True

    time_cnt = 2
    while time_cnt > 0:
        print("running...", str(allowed_external))
        time.sleep(1)
        time_cnt -= 1

    if allowed_external:
        webApp.run(
            host='0.0.0.0',
            port=9090,
            threaded=True,
            use_reloader=False
        )
    else:
        webApp.run(
            host='127.0.0.1',
            port=9090,
            threaded=True,
            use_reloader=False
        )
