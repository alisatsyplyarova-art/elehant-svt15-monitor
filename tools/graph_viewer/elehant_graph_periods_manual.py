
import csv
import tkinter as tk
from tkinter import ttk, messagebox
from pathlib import Path
from datetime import datetime, timedelta
from calendar import monthrange
from collections import defaultdict

import matplotlib.pyplot as plt
import matplotlib.dates as mdates


CSV_FILE = Path(r"C:\Elehant\elehant_history.csv")
DATE_FORMAT = "%Y-%m-%d %H:%M"
MAX_POINTS = 5000

TARIFF_COLORS = ["#ff8c00", "#1687d9", "#2ca02c", "#9467bd"]
PERIODS = ("Час", "День", "Неделя", "Месяц", "Год")

TARIFF_NAMES = {
    1: "Горячая вода",
    2: "Холодная вода",
}


def parse_float(value):
    """Преобразование числа с поддержкой десятичной запятой."""
    return float(str(value).strip().replace(",", "."))


def load_data():
    """Загрузка истории CSV с удалением полностью одинаковых записей."""
    if not CSV_FILE.exists():
        raise FileNotFoundError(f"Не найден файл: {CSV_FILE}")

    rows = []
    seen = set()

    with CSV_FILE.open("r", encoding="utf-8-sig", newline="") as file:
        reader = csv.DictReader(file, delimiter=";")

        if not reader.fieldnames:
            raise RuntimeError("В CSV отсутствует заголовок столбцов.")

        # Нормализуем имена столбцов, сохраняя оригинальные значения.
        columns = {
            name.strip().lower(): name
            for name in reader.fieldnames
            if name
        }

        def get_value(record, *names, default=""):
            for name in names:
                actual = columns.get(name.lower())
                if actual is not None:
                    value = record.get(actual)
                    if value is not None and str(value).strip() != "":
                        return str(value).strip()
            return default

        for record in reader:
            try:
                date_text = get_value(record, "date")
                time_text = get_value(record, "time")

                dt = datetime.strptime(
                    f"{date_text} {time_text}",
                    "%Y-%m-%d %H:%M:%S"
                )

                serial = get_value(record, "serial")
                tariff = int(get_value(record, "tariff"))
                mac = get_value(record, "mac")
                volume = parse_float(
                    get_value(
                        record,
                        "volume_m3",
                        "volume",
                        "volume_m3 "
                    )
                )
                temperature_text = get_value(
                    record, "temperature", default="0"
                )
                temperature = parse_float(temperature_text)

                key = (
                    dt, serial, tariff, mac,
                    volume, temperature
                )

                if key in seen:
                    continue
                seen.add(key)

                rows.append({
                    "dt": dt,
                    "serial": serial,
                    "tariff": tariff,
                    "mac": mac,
                    "volume_m3": volume,
                    "temperature": temperature,
                })

            except (ValueError, TypeError, KeyError):
                # Некорректные или неполные строки пропускаем.
                continue

    rows.sort(key=lambda row: row["dt"])
    return rows


def interval_consumption(rows, tariff):
    """
    Рассчитать расход между соседними показаниями тарифа.
    Отрицательные изменения и повторные временные отметки пропускаются.
    """
    selected = sorted(
        (row for row in rows if row["tariff"] == tariff),
        key=lambda row: row["dt"]
    )

    intervals = []

    for previous, current in zip(selected, selected[1:]):
        seconds = (
            current["dt"] - previous["dt"]
        ).total_seconds()

        difference_m3 = (
            current["volume_m3"] - previous["volume_m3"]
        )

        if seconds <= 0 or difference_m3 < 0:
            continue

        liters = difference_m3 * 1000
        liters_per_hour = liters * 3600 / seconds

        # Отсекаем явно неправдоподобные скачки показаний.
        if liters_per_hour > 10000:
            continue

        intervals.append((
            previous["dt"],
            current["dt"],
            liters,
        ))

    return intervals


def add_months(dt, months):
    """Прибавить количество месяцев с корректировкой дня месяца."""
    month_index = dt.month - 1 + months
    year = dt.year + month_index // 12
    month = month_index % 12 + 1
    day = min(dt.day, monthrange(year, month)[1])

    return dt.replace(year=year, month=month, day=day)


def bucket_floor(dt, period):
    """Начало временного интервала графика."""
    if period == "Час":
        minute = (dt.minute // 5) * 5
        return dt.replace(minute=minute, second=0, microsecond=0)

    if period == "День":
        return dt.replace(minute=0, second=0, microsecond=0)

    if period in ("Неделя", "Месяц"):
        return dt.replace(hour=0, minute=0, second=0, microsecond=0)

    if period == "Год":
        return dt.replace(
            month=1, day=1,
            hour=0, minute=0, second=0, microsecond=0
        )

    raise ValueError(f"Неизвестный период: {period}")


def bucket_next(dt, period):
    """Начало следующего интервала графика."""
    if period == "Час":
        return dt + timedelta(minutes=5)

    if period == "День":
        return dt + timedelta(hours=1)

    if period in ("Неделя", "Месяц"):
        return dt + timedelta(days=1)

    if period == "Год":
        return add_months(dt, 1)

    raise ValueError(f"Неизвестный период: {period}")


def make_series(rows, tariff, period, start, end_exclusive):
    """Распределить расход пропорционально времени внутри интервалов."""
    intervals = interval_consumption(rows, tariff)
    totals = defaultdict(float)

    for interval_start, interval_end, liters in intervals:
        duration = (
            interval_end - interval_start
        ).total_seconds()

        if duration <= 0:
            continue

        overlap_start = max(interval_start, start)
        overlap_end = min(interval_end, end_exclusive)

        if overlap_start >= overlap_end:
            continue

        cursor = bucket_floor(overlap_start, period)

        while cursor < overlap_end:
            next_bucket = bucket_next(cursor, period)
            segment_start = max(overlap_start, cursor)
            segment_end = min(overlap_end, next_bucket)

            seconds = (
                segment_end - segment_start
            ).total_seconds()

            if seconds > 0:
                totals[cursor] += liters * seconds / duration

            cursor = next_bucket

    times = []
    amounts = []
    cursor = bucket_floor(start, period)

    while cursor < end_exclusive:
        times.append(cursor)
        amounts.append(totals.get(cursor, 0.0))
        cursor = bucket_next(cursor, period)

        if len(times) > MAX_POINTS:
            raise RuntimeError(
                "Слишком много точек графика. "
                "Выберите более крупный период "
                "или сократите диапазон дат."
            )

    return times, amounts


def default_range(period, latest):
    """Период по умолчанию для выбранного режима."""
    if period == "Час":
        start = latest.replace(
            minute=0, second=0, microsecond=0
        )
        end = start + timedelta(hours=1)

    elif period == "День":
        start = latest.replace(
            hour=0, minute=0, second=0, microsecond=0
        )
        end = start + timedelta(days=1)

    elif period == "Неделя":
        start = latest.replace(
            hour=0, minute=0, second=0, microsecond=0
        )
        start -= timedelta(days=start.weekday())
        end = start + timedelta(days=7)

    elif period == "Месяц":
        start = latest.replace(
            day=1, hour=0, minute=0,
            second=0, microsecond=0
        )
        end = add_months(start, 1)

    else:
        start = latest.replace(
            month=1, day=1, hour=0, minute=0,
            second=0, microsecond=0
        )
        end = start.replace(year=start.year + 1)

    # Последняя минута выбранного периода включается.
    return start, end - timedelta(minutes=1)


def parse_user_date(text, field_name):
    try:
        return datetime.strptime(text.strip(), DATE_FORMAT)
    except ValueError:
        raise ValueError(
            f"Поле «{field_name}»: неверный формат даты.\n"
            "Нужно ГГГГ-ММ-ДД ЧЧ:ММ, "
            "например 2026-10-01 00:00"
        )


def show_graph():
    try:
        rows = load_data()

        if not rows:
            raise RuntimeError("В CSV нет корректных данных.")

        period = period_var.get()
        start = parse_user_date(start_var.get(), "Начало")
        end = parse_user_date(end_var.get(), "Конец")

        if end < start:
            raise ValueError(
                "Конец периода не может быть раньше начала."
            )

        # Конец включается в период целиком.
        end_exclusive = end + timedelta(minutes=1)

        duration_days = (
            end_exclusive - start
        ).total_seconds() / 86400

        tariffs = sorted({row["tariff"] for row in rows})
        tariff_totals = {}

        fig, ax = plt.subplots(figsize=(13, 8))
        plotted = False

        for index, tariff in enumerate(tariffs):
            times, liters = make_series(
                rows, tariff, period, start, end_exclusive
            )

            if not times:
                continue

            tariff_totals[tariff] = sum(liters)

            label = TARIFF_NAMES.get(
                tariff, f"Тариф {tariff}"
            )

            ax.plot(
                times,
                liters,
                marker="o",
                markersize=4,
                linewidth=1.8,
                label=label,
                color=TARIFF_COLORS[
                    index % len(TARIFF_COLORS)
                ],
            )

            plotted = True

        if not plotted:
            plt.close(fig)
            raise RuntimeError(
                "Нет данных для построения графика "
                "в выбранном периоде."
            )

        if not any(
            any(value > 0 for value in line.get_ydata())
            for line in ax.get_lines()
        ):
            ax.text(
                0.5, 0.5,
                "Нет расхода за выбранный период",
                transform=ax.transAxes,
                ha="center",
                va="center",
                fontsize=14,
                color="#888888",
            )

        # Сводка расхода за выбранный диапазон.
        hot = tariff_totals.get(1, 0.0)
        cold = tariff_totals.get(2, 0.0)
        total = sum(tariff_totals.values())

        hot_daily = hot / duration_days if duration_days > 0 else 0
        cold_daily = cold / duration_days if duration_days > 0 else 0
        total_daily = total / duration_days if duration_days > 0 else 0

        if cold > 0:
            ratio = f"{hot / cold:.2f}:1"
        else:
            ratio = "не определяется: расход холодной воды равен нулю"

        summary = (
            f"Горячая вода: {hot:.2f} л    |    "
            f"Холодная вода: {cold:.2f} л    |    "
            f"Всего: {total:.2f} л\n"
            f"Среднее за сутки: горячая {hot_daily:.2f} л    |    "
            f"холодная {cold_daily:.2f} л    |    "
            f"всего {total_daily:.2f} л\n"
            f"Соотношение горячей к холодной: {ratio}"
        )

        # Последние сохранённые показания по каждому тарифу.
        latest_lines = []
        for tariff in (1, 2):
            tariff_rows = [row for row in rows if row["tariff"] == tariff]
            tariff_name = TARIFF_NAMES.get(tariff, f"Тариф {tariff}")
            if tariff_rows:
                latest_row = max(tariff_rows, key=lambda row: row["dt"])
                latest_lines.append(
                    f"{tariff_name}: {latest_row['volume_m3']:.3f} м³"
                    f"  |  {latest_row['temperature']:.2f} °C"
                    f"  |  {latest_row['dt'].strftime('%d.%m.%Y %H:%M:%S')}"
                )
            else:
                latest_lines.append(f"{tariff_name}: нет сохранённых показаний")

        latest_readings = (
            "Последние сохранённые показания:\n" + "\n".join(latest_lines)
        )

        mode_names = {
            "Час": ("за час, шаг 5 минут", "Время", "%d.%m %H:%M"),
            "День": ("по часам", "Время", "%d.%m %H:%M"),
            "Неделя": ("по дням", "Дата", "%d.%m"),
            "Месяц": ("по дням", "Дата", "%d.%m"),
            "Год": ("по месяцам", "Месяц", "%m.%Y"),
        }

        description, xlabel, xformat = mode_names[period]

        ax.set_title(
            f"СВТ-15 — расход воды ({period.lower()}: {description})\n"
            f"{start.strftime(DATE_FORMAT)} — "
            f"{end.strftime(DATE_FORMAT)}"
        )

        ax.set_xlabel(xlabel)
        ax.xaxis.set_major_locator(
            mdates.AutoDateLocator(maxticks=20)
        )
        ax.xaxis.set_major_formatter(
            mdates.DateFormatter(xformat)
        )
        ax.set_ylabel("Израсходовано воды, литров")
        ax.grid(True, alpha=0.3)
        ax.legend()

        fig.autofmt_xdate(rotation=45)

        # Место снизу для сводки расхода и последних показаний.
        fig.tight_layout(rect=(0, 0.27, 1, 0.96))
        fig.text(
            0.5, 0.105,
            summary,
            ha="center",
            va="bottom",
            fontsize=9,
        )
        latest_text = fig.text(
            0.5, 0.025,
            latest_readings,
            ha="center",
            va="bottom",
            fontsize=9,
        )

        # Каждые 60 секунд перечитываем CSV и обновляем блок показаний.
        def refresh_latest_readings():
            try:
                refreshed_rows = load_data()
                refreshed_lines = []

                for tariff in (1, 2):
                    tariff_rows = [
                        row for row in refreshed_rows
                        if row["tariff"] == tariff
                    ]
                    tariff_name = TARIFF_NAMES.get(
                        tariff, f"Тариф {tariff}"
                    )

                    if tariff_rows:
                        latest_row = max(
                            tariff_rows, key=lambda row: row["dt"]
                        )
                        refreshed_lines.append(
                            f"{tariff_name}: "
                            f"{latest_row['volume_m3']:.3f} м³"
                            f"  |  {latest_row['temperature']:.2f} °C"
                            f"  |  "
                            f"{latest_row['dt'].strftime('%d.%m.%Y %H:%M:%S')}"
                        )
                    else:
                        refreshed_lines.append(
                            f"{tariff_name}: нет сохранённых показаний"
                        )

                latest_text.set_text(
                    "Последние сохранённые показания:\\n"
                    + "\\n".join(refreshed_lines)
                )
            except Exception as refresh_error:
                latest_text.set_text(
                    "Последние сохранённые показания:\\n"
                    f"Не удалось обновить данные: {refresh_error}"
                )

            fig.canvas.draw_idle()
            return True

        refresh_timer = fig.canvas.new_timer(interval=60_000)
        refresh_timer.add_callback(refresh_latest_readings)
        refresh_timer.start()
        # Сохраняем ссылку, чтобы таймер не был удалён сборщиком мусора.
        fig._latest_readings_timer = refresh_timer

        plt.show()

    except Exception as exc:
        messagebox.showerror(
            "Ошибка построения графика", str(exc)
        )


def fill_default_range(*_):
    """Подставить период по умолчанию для выбранного режима."""
    try:
        rows = load_data()
        latest = (
            max(row["dt"] for row in rows)
            if rows else datetime.now()
        )
    except Exception:
        latest = datetime.now()

    start, end = default_range(period_var.get(), latest)
    start_var.set(start.strftime(DATE_FORMAT))
    end_var.set(end.strftime(DATE_FORMAT))


def main():
    global period_var, start_var, end_var

    root = tk.Tk()
    root.title("СВТ-15 — расход воды")
    root.geometry("600x380")
    root.resizable(False, False)

    frame = ttk.Frame(root, padding=22)
    frame.pack(fill="both", expand=True)

    ttk.Label(
        frame,
        text="СВТ-15 — расход воды",
        font=("Segoe UI", 16, "bold"),
    ).pack(pady=(4, 8))

    ttk.Label(
        frame,
        text="Источник данных: C:\\Elehant\\elehant_history.csv",
    ).pack(pady=(0, 12))

    form = ttk.Frame(frame)
    form.pack()

    period_var = tk.StringVar(value="День")
    start_var = tk.StringVar()
    end_var = tk.StringVar()

    ttk.Label(
        form, text="Режим графика:"
    ).grid(row=0, column=0, sticky="e", padx=(0, 8), pady=5)

    combo = ttk.Combobox(
        form,
        textvariable=period_var,
        values=PERIODS,
        state="readonly",
        width=18,
    )
    combo.grid(row=0, column=1, sticky="w", pady=5)
    combo.bind("<<ComboboxSelected>>", fill_default_range)

    ttk.Label(
        form, text="Начало периода:"
    ).grid(row=1, column=0, sticky="e", padx=(0, 8), pady=5)

    ttk.Entry(
        form, textvariable=start_var, width=21
    ).grid(row=1, column=1, sticky="w", pady=5)

    ttk.Label(
        form, text="Конец периода:"
    ).grid(row=2, column=0, sticky="e", padx=(0, 8), pady=5)

    ttk.Entry(
        form, textvariable=end_var, width=21
    ).grid(row=2, column=1, sticky="w", pady=5)

    ttk.Label(
        form,
        text="Формат: ГГГГ-ММ-ДД ЧЧ:ММ",
        foreground="#555555",
    ).grid(row=3, column=1, sticky="w")

    buttons = ttk.Frame(frame)
    buttons.pack(pady=16)

    ttk.Button(
        buttons,
        text="Показать график",
        command=show_graph,
    ).pack(side="left", padx=6)

    ttk.Button(
        buttons,
        text="Сбросить период",
        command=fill_default_range,
    ).pack(side="left", padx=6)

    ttk.Label(
        frame,
        text=(
            "Режим задаёт шаг: час — по 5 минутам, день — по часам,\n"
            "неделя и месяц — по дням, год — по месяцам.\n"
            "При смене режима период подставляется автоматически, "
            "его можно изменить вручную."
        ),
        foreground="#555555",
        justify="center",
    ).pack()

    fill_default_range()
    root.mainloop()


if __name__ == "__main__":
    main()