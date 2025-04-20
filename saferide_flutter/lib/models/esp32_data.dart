class ESP32Data {
  final String timestamp;
  final String status;
  final Map<String, dynamic> sensorData;
  final double accelerationX;
  final double accelerationY;
  final double accelerationZ;
  final double gyroX;
  final double gyroY;
  final double gyroZ;

  ESP32Data({
    required this.timestamp,
    required this.status,
    required this.sensorData,
    required this.accelerationX,
    required this.accelerationY,
    required this.accelerationZ,
    required this.gyroX,
    required this.gyroY,
    required this.gyroZ,
  });

  factory ESP32Data.fromJson(Map<String, dynamic> json) {
    return ESP32Data(
      timestamp: json['timestamp'] ?? '',
      status: json['status'] ?? '',
      sensorData: json['sensorData'] ?? {},
      accelerationX: (json['sensorData']?['accelerationX'] ?? 0.0).toDouble(),
      accelerationY: (json['sensorData']?['accelerationY'] ?? 0.0).toDouble(),
      accelerationZ: (json['sensorData']?['accelerationZ'] ?? 0.0).toDouble(),
      gyroX: (json['sensorData']?['gyroX'] ?? 0.0).toDouble(),
      gyroY: (json['sensorData']?['gyroY'] ?? 0.0).toDouble(),
      gyroZ: (json['sensorData']?['gyroZ'] ?? 0.0).toDouble(),
    );
  }

  Map<String, dynamic> toJson() {
    return {
      'timestamp': timestamp,
      'sensorData': sensorData,
      'status': status,
    };
  }
} 