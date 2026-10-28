#pragma once

#include <filesystem>

/**
 * Фиксация файла в том же каталоге.
 *
 * Барьер данных — fsync самого файла. Барьер каталога — fsync каталога
 * после публикации имени. rename сам по себе барьером от потери питания
 * не считается.
 *
 * PublishNewFile не заменяет существующее имя: исследование с тем же
 * путём не перезаписывается. PublishReplacingFile заменяет только служебный
 * файл той же сессии (manifest.json), и только после fsync временной копии.
 *
 * Это описание локальной файловой системы, которая выполняет fsync.
 * Оно не является замером диска сканера.
 */

bool FsyncFile(const std::filesystem::path &path);

bool FsyncDirectory(const std::filesystem::path &directory);

bool PublishNewFile(const std::filesystem::path &temp_path, const std::filesystem::path &final_path);

bool PublishReplacingFile(const std::filesystem::path &temp_path, const std::filesystem::path &final_path);
