# -*- coding:utf-8 -*-
import os
import json

# from flask import Flask, Response, request, jsonify
import flask
import time
import pathlib
from apscheduler.schedulers.background import BackgroundScheduler

import RPi.GPIO as GPIO


app = flask.Flask(__name__)
job = None


def set_medicine_data():
    current_path = os.path.dirname(os.path.abspath(__file__))
    if (pathlib.Path(current_path + "/saved_data.json")).exists():
        with open(current_path + "/saved_data.json", "r") as jsonFile:
            data = json.load(jsonFile)

        data["Medicine"] = 1

        with open(current_path + "/saved_data.json", "w") as jsonFile:
            json.dump(data, jsonFile)


def _turnoff():
    GPIO.output(17, GPIO.HIGH)


def _turnon():
    GPIO.output(17, GPIO.HIGH)
    time.sleep(1)
    GPIO.output(17, GPIO.LOW)


def button_callback(channel):
    print("Button was pushed!")
    _turnoff()
    set_medicine_data()


def update_everyday_job(hour=8, minute=36):
    global job

    if job != None:
        job.remove_job("turnon_job")
        job = None
        print("remove background job")

    job = BackgroundScheduler(timezone="Asia/Seoul")
    job.add_job(_turnon, "cron", hour=hour, minute=minute, id="turnon_job")
    job.start()
    print(
        "BackgroundScheduler started: ", str(hour), str(minute), " everyday; ", str(job)
    )


@app.route("/")
def _home():
    print(
        "api_server request: "
        + str(flask.request)
        + " body: "
        + str(flask.request.data)
    )
    return "Hello, api_server World!\n"


@app.route("/data", methods=["GET"])
def index():
    acc_x = ""
    acc_y = ""
    acc_z = ""
    medicine = ""
    alarm_time = ""
    # Category = flask.request.args.get("Category")

    current_path = os.path.dirname(os.path.abspath(__file__))
    if (pathlib.Path(current_path + "/saved_data.json")).exists():
        with open(current_path + "/saved_data.json", "r", encoding="utf8") as cf:
            data = json.load(cf)
            acc_x = data.get("Accel_X", "")
            acc_y = data.get("Accel_Y", "")
            acc_z = data.get("Accel_Z", "")
            medicine = data.get("Medicine", 0)

    if (pathlib.Path(current_path + "/medicine.json")).exists():
        with open(current_path + "/medicine.json", "r", encoding="utf8") as cf:
            data = json.load(cf)
            alarm_time = data.get("time", "")

    return flask.jsonify(
        {
            "acc_x": acc_x,
            "acc_y": acc_y,
            "acc_z": acc_z,
            "medicine_completion": medicine,
            "alarm_time": alarm_time,
        }
    )


# curl -X PUT -H Content-Type:application/json -d @control.json http://192.168.0.3:9090/control
@app.route("/control", methods=["PUT"])
def _control():
    print(
        "api_server /control request: "
        + str(flask.request)
        + " body: "
        + str(flask.request.data)
    )
    ctrl = flask.request.get_json()
    h = ctrl["time"].split(":")[0]
    m = ctrl["time"].split(":")[1]
    print("api_server /control json: " + str(ctrl), h, m)

    current_path = os.path.dirname(os.path.abspath(__file__))
    if (pathlib.Path(current_path + "/medicine.json")).exists():
        with open(current_path + "/medicine.json", "w", encoding="utf8") as cf:
            try:
                json.dump(ctrl, cf)
                update_everyday_job(h, m)
                ret = {"status": "ok"}
            except Exception as e:
                print("failed to write medicine.json")
                ret = {"status": "error"}

    return flask.jsonify(ret)


if __name__ == "__main__":
    allowed_external = True

    time_cnt = 2
    while time_cnt > 0:
        print("running...", str(allowed_external))
        time.sleep(1)
        time_cnt -= 1

    GPIO.setmode(GPIO.BCM)
    GPIO.setup(17, GPIO.OUT)
    GPIO.output(17, GPIO.HIGH)
    GPIO.setup(27, GPIO.IN, pull_up_down=GPIO.PUD_UP)
    GPIO.add_event_detect(27, GPIO.RISING, callback=button_callback, bouncetime=200)

    update_everyday_job()

    if allowed_external:
        app.run(host="0.0.0.0", port=9090, threaded=True, use_reloader=False)
    else:
        app.run(host="127.0.0.1", port=9090, threaded=True, use_reloader=False)
