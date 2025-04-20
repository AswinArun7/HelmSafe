import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:google_fonts/google_fonts.dart';
import '../providers/esp32_provider.dart';

class SettingsScreen extends StatelessWidget {
  const SettingsScreen({super.key});

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Settings'),
      ),
      body: Consumer<ESP32Provider>(
        builder: (context, esp32Provider, child) {
          return Padding(
            padding: const EdgeInsets.all(16.0),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const Text(
                  'ESP32 IP Address',
                  style: TextStyle(
                    fontSize: 18,
                    fontWeight: FontWeight.bold,
                  ),
                ),
                const SizedBox(height: 8),
                TextField(
                  decoration: InputDecoration(
                    hintText: 'Enter ESP32 IP address',
                    border: const OutlineInputBorder(),
                    suffixIcon: IconButton(
                      icon: const Icon(Icons.save),
                      onPressed: () {
                        final ipController = TextEditingController(
                          text: esp32Provider.ipAddress,
                        );
                        showDialog(
                          context: context,
                          builder: (context) => AlertDialog(
                            title: const Text('Save IP Address'),
                            content: TextField(
                              controller: ipController,
                              decoration: const InputDecoration(
                                labelText: 'IP Address',
                                hintText: 'e.g., 192.168.1.100',
                              ),
                            ),
                            actions: [
                              TextButton(
                                onPressed: () => Navigator.pop(context),
                                child: const Text('Cancel'),
                              ),
                              TextButton(
                                onPressed: () async {
                                  esp32Provider.setIpAddress(ipController.text);
                                  Navigator.pop(context);
                                  // Test connection after setting new IP
                                  final success = await esp32Provider.testConnection();
                                  if (!success && context.mounted) {
                                    ScaffoldMessenger.of(context).showSnackBar(
                                      SnackBar(
                                        content: Text(esp32Provider.lastError ?? 'Connection failed'),
                                        backgroundColor: Colors.red,
                                      ),
                                    );
                                  }
                                },
                                child: const Text('Save'),
                              ),
                            ],
                          ),
                        );
                      },
                    ),
                  ),
                  controller: TextEditingController(text: esp32Provider.ipAddress),
                  readOnly: true,
                ),
                const SizedBox(height: 16),
                Card(
                  child: Padding(
                    padding: const EdgeInsets.all(16.0),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Row(
                          children: [
                            const Text(
                              'Connection Status: ',
                              style: TextStyle(fontSize: 16),
                            ),
                            Text(
                              esp32Provider.isConnected ? 'Connected' : 'Disconnected',
                              style: TextStyle(
                                color: esp32Provider.isConnected ? Colors.green : Colors.red,
                                fontSize: 16,
                                fontWeight: FontWeight.bold,
                              ),
                            ),
                          ],
                        ),
                        if (esp32Provider.lastError != null) ...[
                          const SizedBox(height: 8),
                          Text(
                            'Error: ${esp32Provider.lastError}',
                            style: const TextStyle(
                              color: Colors.red,
                              fontSize: 14,
                            ),
                          ),
                        ],
                        if (esp32Provider.currentData != null) ...[
                          const SizedBox(height: 8),
                          Text(
                            'Last Update: ${DateTime.now().toString()}',
                            style: const TextStyle(fontSize: 14),
                          ),
                          const SizedBox(height: 4),
                          Text(
                            'Helmet: ${esp32Provider.currentData!.isHelmetOn ? "Worn" : "Not Worn"}',
                            style: const TextStyle(fontSize: 14),
                          ),
                          Text(
                            'Alcohol Level: ${esp32Provider.currentData!.alcoholLevel.toStringAsFixed(2)}',
                            style: const TextStyle(fontSize: 14),
                          ),
                        ],
                      ],
                    ),
                  ),
                ),
                const SizedBox(height: 16),
                ElevatedButton.icon(
                  onPressed: () async {
                    final success = await esp32Provider.testConnection();
                    if (!success && context.mounted) {
                      ScaffoldMessenger.of(context).showSnackBar(
                        SnackBar(
                          content: Text(esp32Provider.lastError ?? 'Connection failed'),
                          backgroundColor: Colors.red,
                        ),
                      );
                    }
                  },
                  icon: const Icon(Icons.wifi_tethering),
                  label: const Text('Test Connection'),
                ),
              ],
            ),
          );
        },
      ),
    );
  }
} 