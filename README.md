# Cache emul

Основная библиотека - `cache_emul/oracl_lib`

Бинарь для запуска эксперементов - `cache_emul/experiments`

Скрипт для теоретической оценки кэша в датаценте - `cache_emul/calc_dc_cache`

## Как запустить эксперимент

Пример конфига эксперемента: `cache_emul/experiments/configs/diff_config_prod_generator.proto.txt`

Для использования синтетического генератора - достаточно собрать и запустить:

```
cd cache_emul/experiments/bin
ya make -r
./bin grid -p ../configs/diff_config_prod_generator.proto.txt
```

Для использования собранных запросов (FileGenerator) нужен файл в формате:
```
8 byte - число патрон

далее патроны:
8 byte - timestamp
8 byte - cache key
8 byte - dc balancing hash
```
Можно скачать данные дневных походов в Yandex Search [отсуда](https://zenodo.org/records/15389143)

Далее указать в конфиге путь к файлу и также собрать и запустить.

## Стоит подкоректировать конфигурацию кластера, если использовать дефолт из конфига может потребоваться до 70GB RAM!
