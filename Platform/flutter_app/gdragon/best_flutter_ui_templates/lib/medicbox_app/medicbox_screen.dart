import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;

class MedicBoxScreen extends StatefulWidget {
  @override
  _MedicBoxScreenState createState() => _MedicBoxScreenState();
}

class _MedicBoxScreenState extends State<MedicBoxScreen> {
  Future<MedicBox> medicbox;

  @override
  void initState() {
    super.initState();
    medicbox = fetchStatus();
  }

  @override
  Widget build(BuildContext context) {
    Size size = MediaQuery.of(context).size;
    return Scaffold(
      appBar: AppBar(
        title: Text('복용 알람'),
      ),
      body: Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: <Widget>[
            SizedBox(height: size.height * 0.03),
            FutureBuilder<MedicBox>(
              future: medicbox,
              builder: (context, snapshot) {
                var ret;
                if (snapshot.hasData) {
                  ret = snapshot.data.medicineCompleted;
                  if (ret == '' || ret != 1) {
                    ret = '오늘은 아직 약을 복용하지 않았습니다.';
                  } else {
                    ret = '오늘은 약을 복용 했습니다. 완료. 👌';
                  }
                } else if (snapshot.hasError) {
                  ret = "${snapshot.error}";
                  print('FutureBuilder, snapshot.hasError: ' + ret.toString());
                } else {
                  ret = '처리중';
                  print('FutureBuilder' + ret.toString());
                  return CircularProgressIndicator();
                }
                return Padding(
                  padding: EdgeInsets.symmetric(horizontal: size.width * 0.05),
                  child: Text(
                    ret,
                    style: Theme.of(context)
                        .textTheme
                        .headline5
                        .copyWith(fontWeight: FontWeight.bold),
                  ),
                );
              },
            ),
            Padding(
              padding: EdgeInsets.symmetric(horizontal: size.width * 0.05),
              child: ElevatedButton(
                  onPressed: () {
                    fetchStatus();
                  },
                  child: Text('새로 고침')),
            ),
            Padding(
              padding: EdgeInsets.symmetric(horizontal: size.width * 0.05),
              child: ElevatedButton(
                  onPressed: () {
                    Future<TimeOfDay> future = showTimePicker(
                      context: context,
                      initialTime: TimeOfDay.now(),
                    );
                    future.then((timeOfDay) {
                      var t = timeOfDay.hour.toString() +
                          ":" +
                          timeOfDay.minute.toString();
                      updateMedicine(t);
                    });
                  },
                  child: Text('알람 설정')),
            )
          ],
        ),
      ),
    );
  }
}

Future<MedicBox> fetchStatus() async {
  final response = await http.get('http://192.168.0.3:9090/data');

  if (response.statusCode == 200) {
    return MedicBox.fromJson(json.decode(response.body));
  } else {
    print(response.statusCode);
    throw Exception('Failed to load post');
  }
}

Future<MedicBox> updateMedicine(String time) async {
  final response = await http.put(
    'http://192.168.0.3:9090/control',
    headers: <String, String>{
      'Content-Type': 'application/json; charset=UTF-8',
    },
    body: jsonEncode(<String, dynamic>{
      'time': time,
    }),
  );

  if (response.statusCode == 200) {
    return MedicBox.fromJson(json.decode(response.body));
  } else {
    print(response.statusCode);
    throw Exception('Failed to load post');
  }
}

Future<http.Response> createAlbum(String title) {
  return http.post(
    Uri.parse('https://jsonplaceholder.typicode.com/albums'),
    headers: <String, String>{
      'Content-Type': 'application/json; charset=UTF-8',
    },
    body: jsonEncode(<String, String>{
      'title': title,
    }),
  );
}

class MedicBox {
  final int accX;
  final int accY;
  final int accZ;
  final String medicineCompleted;
  final String medicineTime;

  MedicBox(
      {this.accX,
      this.accY,
      this.accZ,
      this.medicineCompleted,
      this.medicineTime});

  factory MedicBox.fromJson(Map<String, dynamic> json) {
    return MedicBox(
        accX: json['acc_x'],
        accY: json['acc_y'],
        accZ: json['acc_z'],
        medicineCompleted: json['medicine_completion'],
        medicineTime: json['alarm_time']);
  }
}
