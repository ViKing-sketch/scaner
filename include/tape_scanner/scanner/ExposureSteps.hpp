#pragma once

#include <string>
#include <vector>

/**
 * Таблица шагов экспозиции, микросекунды.
 *
 * Запрошенное время и время, которое реально несёт кадр, не совпадают:
 * у CMOS экспозиция не непрерывна. Макс: запрос 40–69 мкс даёт кадр как 70 мкс.
 *
 * Сейчас в коде таблица-заглушка. Это не калибровка.
 * PLACEHOLDER: awaiting Max's measured exposure step table. Not a calibration.
 * Единственная наблюдённая строка — пример Макса. Остальные границы
 * стоят, чтобы у отображения была форма, и помечены как placeholder.
 * Запрос вне таблицы не подменяется выдуманным шагом: эффективное время
 * остаётся запрошенным.
 *
 * Команда камере остаётся запрошенным временем. Эффективное время пишется
 * в журнал, в метаданные сессии и в веса HDR.
 */

inline constexpr const char kExposureStepTablePlaceholderNote[] =
    "PLACEHOLDER: awaiting Max's measured exposure step table. Not a calibration.";

enum class ExposureStepOrigin {
  /// Граница не измерена. Не калибровка.
  Placeholder,
  /// Пример Макса: запрос 40–69 мкс даёт кадр как 70 мкс. Это не полная таблица.
  ObservedExample,
};

struct ExposureStep {
  double requested_min_us = 0; // включительно
  double requested_max_us = 0; // включительно
  double effective_us = 0;
  ExposureStepOrigin origin = ExposureStepOrigin::Placeholder;
};

struct ExposureMatch {
  double requested_us = 0;
  double effective_us = 0;
  /// Индекс строки таблицы. -1: запрос вне таблицы, effective_us == requested_us.
  int step_index = -1;
};

struct ExposureObservation {
  double requested_us = 0;
  double effective_us = 0;
  int step_index = -1;
};

struct SessionExposureMetadata {
  bool table_placeholder = true;
  std::string table_note;
  std::vector<ExposureObservation> exposures;
};

class ExposureStepTable {
public:
  ExposureStepTable(std::vector<ExposureStep> steps, std::string note, bool placeholder);

  /// Заглушка. Не калибровка. Ждёт измеренную таблицу Макса.
  static const ExposureStepTable &Placeholder();

  ExposureMatch Match(double requested_us, bool log = true) const;
  double EffectiveUs(double requested_us, bool log = true) const;

  const std::vector<ExposureStep> &Steps() const { return m_steps; }
  const std::string &Note() const { return m_note; }
  bool IsPlaceholder() const { return m_placeholder; }

private:
  std::vector<ExposureStep> m_steps;
  std::string m_note;
  bool m_placeholder = true;
};

/// Таблица, которой пользуются камера, HDR и сессия, пока нет замера Макса.
const ExposureStepTable &ActiveExposureStepTable();

/// Строка для Image Comments и журнала: запрошенные и эффективные микросекунды.
std::string FormatExposureSessionComment(const SessionExposureMetadata &metadata);

/// Пишет JSON метаданных экспозиции. false — файл не опубликован, запись в RAM не отменяется.
bool WriteExposureMetadataFile(const std::string &path, const SessionExposureMetadata &metadata);
