import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'package:flutter/foundation.dart';
import 'package:http/http.dart' as http;
import 'package:shared_preferences/shared_preferences.dart';

class ESP32Data {
  final int timestamp;
  final String status;
  final Map<String, dynamic> sensorData;
  final double accelerationX;
  final double accelerationY;
  final double accelerationZ;
  final double alcoholLevel;
  final bool isHelmetOn;
  final bool isDrunk;
  final bool crashDetected;

  ESP32Data({
    required this.timestamp,
    required this.status,
    required this.sensorData,
    required this.accelerationX,
    required this.accelerationY,
    required this.accelerationZ,
    required this.alcoholLevel,
    required this.isHelmetOn,
    required this.isDrunk,
    required this.crashDetected,
  });

  factory ESP32Data.fromJson(Map<String, dynamic> json) {
    final sensorData = json['sensorData'] as Map<String, dynamic>;
    return ESP32Data(
      timestamp: json['timestamp'] as int? ?? 0,
      status: json['status'] as String? ?? 'OK',
      sensorData: sensorData,
      accelerationX: (sensorData['accelerationX'] ?? 0).toDouble(),
      accelerationY: (sensorData['accelerationY'] ?? 0).toDouble(),
      accelerationZ: (sensorData['accelerationZ'] ?? 0).toDouble(),
      alcoholLevel: (sensorData['alcoholLevel'] ?? 0).toDouble(),
      isHelmetOn: sensorData['isHelmetOn'] as bool? ?? false,
      isDrunk: sensorData['isDrunk'] as bool? ?? false,
      crashDetected: sensorData['crashDetected'] as bool? ?? false,
    );
  }
}

class ESP32Provider with ChangeNotifier {
  bool _isPolling = false;
  bool _hasNotifications = false;
  List<ESP32Data> _data = [];
  Timer? _pollingTimer;
  String? _ipAddress;
  bool _isConnected = false;
  ESP32Data? _currentData;
  String? _lastError;

  String? get ipAddress => _ipAddress;
  bool get isConnected => _isConnected;
  ESP32Data? get currentData => _currentData;
  bool get isPolling => _isPolling;
  bool get hasNotifications => _hasNotifications;
  List<ESP32Data> get data => _data;
  String? get lastError => _lastError;

  ESP32Provider() {
    _ipAddress = '192.168.4.1';
  }

  void setIpAddress(String ip) {
    print('Setting IP address to: $ip');
    if (_ipAddress != ip) {
      _ipAddress = ip;
      _isConnected = false;
      _lastError = null;
      notifyListeners();
      // Test connection immediately after IP change
      testConnection().then((success) {
        print('Initial connection test: ${success ? 'successful' : 'failed'}');
      });
    }
  }

  void _checkForAlerts(ESP32Data newData) {
    bool hasAlert = newData.crashDetected || 
                   newData.isDrunk || 
                   !newData.isHelmetOn ||
                   newData.alcoholLevel > 1000;

    if (hasAlert != _hasNotifications) {
      _hasNotifications = hasAlert;
      notifyListeners();
    }
  }

  Future<bool> testConnection() async {
    if (_ipAddress == null || _ipAddress!.isEmpty) {
      _lastError = 'IP address is not set';
      _isConnected = false;
      notifyListeners();
      return false;
    }

    try {
      print('Testing connection to: http://$_ipAddress/data');
      
      final uri = Uri.parse('http://$_ipAddress/data');
      print('Parsed URI: $uri');

      final client = http.Client();
      try {
        print('Sending HTTP request...');
        final response = await client.get(
          uri,
          headers: {
            'Accept': 'application/json',
            'Content-Type': 'application/json',
          },
        ).timeout(
          const Duration(seconds: 10),
          onTimeout: () {
            print('Connection timed out after 10 seconds');
            throw TimeoutException('Connection timed out');
          },
        );

        print('Response received:');
        print('Status code: ${response.statusCode}');
        print('Headers: ${response.headers}');
        print('Body: ${response.body}');

        if (response.statusCode == 200) {
          try {
            final jsonData = jsonDecode(response.body);
            print('JSON decoded successfully: $jsonData');
            
            final data = ESP32Data.fromJson(jsonData);
            print('Data parsed successfully');
            
            _isConnected = true;
            _lastError = null;
            notifyListeners();
            return true;
          } catch (e) {
            print('JSON parsing error: $e');
            _isConnected = false;
            _lastError = 'Invalid data format: $e';
            notifyListeners();
            return false;
          }
        } else {
          print('Server returned error status: ${response.statusCode}');
          _isConnected = false;
          _lastError = 'Server error: ${response.statusCode}';
          notifyListeners();
          return false;
        }
      } catch (e) {
        print('HTTP request error: $e');
        rethrow;
      } finally {
        client.close();
      }
    } catch (e) {
      print('Connection error: $e');
      _isConnected = false;
      _lastError = 'Connection failed: $e';
      notifyListeners();
      return false;
    }
  }

  Future<void> fetchData() async {
    if (!_isConnected) {
      print('Not connected, attempting to reconnect...');
      final success = await testConnection();
      if (!success) {
        print('Reconnection failed');
        return;
      }
    }

    try {
      print('Fetching data from: http://$_ipAddress/data');
      
      final client = http.Client();
      try {
        final response = await client.get(
          Uri.parse('http://$_ipAddress/data'),
          headers: {
            'Accept': 'application/json',
            'Content-Type': 'application/json',
          },
        ).timeout(
          const Duration(seconds: 5),
        );

        print('Fetch response: ${response.statusCode}');
        print('Response body: ${response.body}');

        if (response.statusCode == 200) {
          try {
            final newData = ESP32Data.fromJson(jsonDecode(response.body));
            _data.insert(0, newData);
            if (_data.length > 100) _data.removeLast();
            _checkForAlerts(newData);
            _currentData = newData;
            _isConnected = true;
            _lastError = null;
            notifyListeners();
          } catch (e) {
            print('Data parsing error: $e');
            _isConnected = false;
            _lastError = 'Invalid data format: $e';
            notifyListeners();
          }
        } else {
          print('Server error: ${response.statusCode}');
          _isConnected = false;
          _lastError = 'Server error: ${response.statusCode}';
          notifyListeners();
        }
      } finally {
        client.close();
      }
    } catch (e) {
      print('Fetch error: $e');
      _isConnected = false;
      _lastError = 'Fetch failed: $e';
      notifyListeners();
    }
  }

  void startPolling() {
    if (!_isPolling) {
      print('Starting data polling');
      _pollingTimer = Timer.periodic(const Duration(seconds: 1), (_) {
        fetchData();
      });
      _isPolling = true;
      notifyListeners();
    }
  }

  void stopPolling() {
    print('Stopping data polling');
    _pollingTimer?.cancel();
    _isPolling = false;
    notifyListeners();
  }

  void togglePolling() {
    if (_isPolling) {
      stopPolling();
    } else {
      startPolling();
    }
  }

  @override
  void dispose() {
    stopPolling();
    super.dispose();
  }
} 