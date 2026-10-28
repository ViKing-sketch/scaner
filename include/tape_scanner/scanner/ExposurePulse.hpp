#pragma once

#include <string>
#include <vector>

/**
 * Экспозиция — длительность импульса подсветки контроллера, не шаг затвора камеры.
 *
 * Для цели T затвор камеры = T + запас. Запас по умолчанию 10 мкс:
 * T = 20 мкс → затвор 30 мкс. Затвор не короче импульса и перекрывает его.
 * Эффективная экспозиция для HDR, метаданных сессии и DICOM — длительность импульса.
 * Значение затвора записывается рядом.
 *
 * Таблица шагов экспозиции (запрос 40–69 мкс → кадр как 70 мкс) снята.
 * DIAGNOSTIC: она не участвует в весах HDR, session-meta и DICOM.
 *
 * Кадр TCP импульса — TODO до Z04. Прошивка импульс уже умеет.
 * Хост команду не выдумывает: боевой контроллер — заглушка,
 * тесты идут через ControllerPulseSimulator.
 */

inline constexpr double kDefaultShutterMarginUs = 10.0;

inline constexpr const char kExposurePulseNote[] =
    "Effective exposure is the controller light-pulse duration. "
    "Camera shutter is pulse + margin and must cover the pulse. "
    "Host TCP framing is TODO until Z04; firmware already has the pulse. "
    "DIAGNOSTIC: the exposure step table is retired and is not used.";

/// Команда импульса подсветки. Реализация сокета — Z04.
class IControllerPulse {
public:
  virtual ~IControllerPulse() = default;

  /// Длительность импульса, микросекунды. Это экспозиция кадра, не затвор.
  /// TODO(Z04): прошивка импульс уже умеет. Кадр TCP здесь не задавать.
  virtual bool CommandLightPulse(double duration_us) = 0;
};

/// Имитатор контроллера без сокета, wx и SDK камеры.
class ControllerPulseSimulator final : public IControllerPulse {
public:
  bool CommandLightPulse(double duration_us) override;
  const std::vector<double> &PulsesUs() const { return m_pulses; }
  void Clear() { m_pulses.clear(); }

private:
  std::vector<double> m_pulses;
};

struct ExposureShot {
  double target_us = 0;
  double pulse_us = 0;
  double shutter_us = 0;
  double margin_us = 0;
  /// План принят: импульс > 0, запас >= 0, затвор >= импульса.
  bool planned = false;
  /// Контроллер принял команду. Заглушка Z04 возвращает false.
  bool pulse_commanded = false;
};

struct SessionExposureMetadata {
  std::string model = "controller-light-pulse";
  std::string note;
  std::vector<ExposureShot> exposures;
};

/// Считает затвор. Контроллер не вызывает.
ExposureShot PlanExposure(double target_us, double margin_us = kDefaultShutterMarginUs);

/// План и команда импульса через интерфейс контроллера.
ExposureShot CommandExposure(IControllerPulse &controller, double target_us,
                             double margin_us = kDefaultShutterMarginUs);

std::string FormatExposureSessionComment(const SessionExposureMetadata &metadata);

bool WriteExposureMetadataFile(const std::string &path, const SessionExposureMetadata &metadata);
