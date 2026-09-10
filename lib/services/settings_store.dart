import 'dart:convert';
import 'dart:io';

import '../models/effect_settings.dart';
import '../models/presets.dart';

/// Reads and writes `%APPDATA%\WindowEffects\config.json`.
///
/// Writes go through a temp file and a rename, so a crash mid-write can never
/// leave a truncated config behind. A file that is missing, unreadable or
/// malformed falls back to the Premium preset instead of refusing to start —
/// a settings utility that won't launch because of its own config file is
/// worse than one that quietly resets.
class SettingsStore {
  SettingsStore({Directory? directory}) : _overrideDirectory = directory;

  static const String _folderName = 'WindowEffects';
  static const String _fileName = 'config.json';
  static const int _schemaVersion = 1;

  final Directory? _overrideDirectory;

  /// Set when the last load failed, for surfacing in the UI.
  String? lastError;

  Directory get directory {
    if (_overrideDirectory != null) return _overrideDirectory;
    final String? appData = Platform.environment['APPDATA'];
    final String base = (appData != null && appData.isNotEmpty)
        ? appData
        : Directory.systemTemp.path;
    return Directory('$base${Platform.pathSeparator}$_folderName');
  }

  File get file => File('${directory.path}${Platform.pathSeparator}$_fileName');

  Future<EffectSettings> load() async {
    lastError = null;
    try {
      final File target = file;
      if (!await target.exists()) return kDefaultSettings;

      String raw = await target.readAsString();
      // A byte-order mark is not JSON, and every Windows editor that offers
      // "UTF-8" writes one — Notepad, and PowerShell's own Set-Content. A user
      // who edits this file by hand should not be told their config is corrupt
      // because of three bytes they cannot see.
      if (raw.startsWith('﻿')) raw = raw.substring(1);
      if (raw.trim().isEmpty) return kDefaultSettings;

      final Object? decoded = jsonDecode(raw);
      if (decoded is! Map) {
        lastError = 'Config file was not an object; defaults restored.';
        return kDefaultSettings;
      }

      final Map<String, Object?> map = decoded.cast<String, Object?>();
      final Object? settings = map['settings'];
      if (settings is! Map) {
        lastError = 'Config file had no settings block; defaults restored.';
        return kDefaultSettings;
      }

      return EffectSettings.fromJson(settings.cast<String, Object?>());
    } on FormatException catch (error) {
      lastError = 'Config file was not valid JSON (${error.message}).';
      return kDefaultSettings;
    } on FileSystemException catch (error) {
      lastError = 'Could not read the config file: ${error.osError?.message ?? error.message}.';
      return kDefaultSettings;
    }
  }

  /// Persists [settings]. Returns null on success, or a message on failure.
  Future<String?> save(EffectSettings settings) async {
    try {
      final Directory dir = directory;
      if (!await dir.exists()) await dir.create(recursive: true);

      final String payload = const JsonEncoder.withIndent('  ').convert(
        <String, Object?>{
          'schemaVersion': _schemaVersion,
          'settings': settings.toJson(),
        },
      );

      final File temp = File('${file.path}.tmp');
      await temp.writeAsString(payload, flush: true);
      await temp.rename(file.path);
      return null;
    } on FileSystemException catch (error) {
      return 'Could not save settings: ${error.osError?.message ?? error.message}.';
    }
  }
}
