import tkinter as tk
from tkinter import ttk, messagebox
import csv
import os
from datetime import datetime

CSV_FILE = r"C:\Elehant\elehant_history.csv"
UPDATE_MS = 2000


# =========================================================
# ДАТА НАЧАЛА РАСЧЁТА
# =========================================================

def get_start_date():
    value = start_date_var.get().strip()

    try:
        return datetime.strptime(
            value,
            "%d.%m.%Y"
        ).date()

    except ValueError:
        return None


# =========================================================
# ЧТЕНИЕ CSV
# =========================================================

def read_csv():
    data = {}

    if not os.path.exists(CSV_FILE):
        return data

    try:
        with open(
            CSV_FILE,
            "r",
            encoding="utf-8-sig",
            newline=""
        ) as f:

            reader = csv.DictReader(
                f,
                delimiter=";"
            )

            for row in reader:

                try:
                    serial = row["serial"]
                    tariff = int(row["tariff"])

                    item = {
                        "date": row["date"],
                        "time": row["time"],
                        "volume": float(row["volume_m3"]),
                        "temperature": float(row["temperature_c"]),
                        "battery": int(row["battery_percent"]),
                        "firmware": row["firmware"],
                        "rssi": int(row["rssi"]),
                    }

                    key = (
                        serial,
                        tariff
                    )

                    if key not in data:
                        data[key] = []

                    data[key].append(item)

                except (
                    ValueError,
                    KeyError
                ):
                    continue

    except (
        PermissionError,
        OSError
    ):
        pass

    return data


# =========================================================
# РАСХОД
# =========================================================

def calculate_consumption(
    rows,
    mode
):

    if not rows:
        return 0.0

    now = datetime.now()

    if mode == "today":

        filtered = [
            r for r in rows
            if r["date"] ==
            now.strftime("%Y-%m-%d")
        ]

    elif mode == "month":

        start_date = get_start_date()

        if start_date is None:
            return 0.0

        filtered = []

        for r in rows:

            try:
                row_date = datetime.strptime(
                    r["date"],
                    "%Y-%m-%d"
                ).date()

            except ValueError:
                continue

            if (
                row_date >= start_date
                and row_date <= now.date()
            ):
                filtered.append(r)

    else:

        filtered = rows

    if len(filtered) < 2:
        return 0.0

    first = filtered[0]["volume"]
    last = filtered[-1]["volume"]

    consumption = last - first

    if consumption < 0:
        return 0.0

    return consumption


# =========================================================
# ОБНОВЛЕНИЕ ТАРИФА
# =========================================================

def update_water_block(
    rows,
    labels
):

    if not rows:
        return 0.0, 0.0

    last = rows[-1]

    volume = last["volume"]
    temperature = last["temperature"]
    battery = last["battery"]
    rssi = last["rssi"]
    last_time = last["time"]

    today = calculate_consumption(
        rows,
        "today"
    )

    month = calculate_consumption(
        rows,
        "month"
    )

    labels["volume"].config(
        text=f"Показания: {volume:.4f} м³"
    )

    labels["today"].config(
        text=f"Расход сегодня: {today:.4f} м³"
    )

    labels["month"].config(
        text=f"Расход за месяц: {month:.4f} м³"
    )

    labels["temperature"].config(
        text=f"Температура: {temperature:.2f} °C"
    )

    labels["battery"].config(
        text=f"Батарея: {battery}%    RSSI: {rssi} dBm"
    )

    labels["time"].config(
        text=f"Последний пакет: {last_time}"
    )

    return today, month


# =========================================================
# БЛОК ТАРИФА
# =========================================================

def create_water_block(
    parent,
    title
):

    frame = ttk.LabelFrame(
        parent,
        text=f" {title} "
    )

    frame.pack(
        fill="x",
        padx=10,
        pady=5
    )

    labels = {}

    labels["volume"] = ttk.Label(
        frame,
        text="Показания: --",
        font=(
            "Segoe UI",
            14,
            "bold"
        )
    )

    labels["volume"].pack(
        anchor="w",
        padx=10,
        pady=(8, 2)
    )

    labels["today"] = ttk.Label(
        frame,
        text="Расход сегодня: --"
    )

    labels["today"].pack(
        anchor="w",
        padx=10
    )

    labels["month"] = ttk.Label(
        frame,
        text="Расход за месяц: --"
    )

    labels["month"].pack(
        anchor="w",
        padx=10
    )

    labels["temperature"] = ttk.Label(
        frame,
        text="Температура: --"
    )

    labels["temperature"].pack(
        anchor="w",
        padx=10,
        pady=(5, 0)
    )

    labels["battery"] = ttk.Label(
        frame,
        text="Батарея: --"
    )

    labels["battery"].pack(
        anchor="w",
        padx=10
    )

    labels["time"] = ttk.Label(
        frame,
        text="Последний пакет: --"
    )

    labels["time"].pack(
        anchor="w",
        padx=10,
        pady=(0, 8)
    )

    return {
        "frame": frame,
        "labels": labels
    }


# =========================================================
# СЧЁТЧИК
# =========================================================

def create_meter(
    parent,
    serial
):

    meter = ttk.LabelFrame(
        parent,
        text=f" СЧЁТЧИК №{serial} "
    )

    meter.pack(
        fill="x",
        padx=10,
        pady=10
    )

    hot = create_water_block(
        meter,
        "🔥 ГОРЯЧАЯ"
    )

    cold = create_water_block(
        meter,
        "❄️ ХОЛОДНАЯ"
    )

    single = create_water_block(
        meter,
        "ПОКАЗАНИЯ"
    )

    total_frame = ttk.LabelFrame(
        meter,
        text=" ИТОГО "
    )

    total_frame.pack(
        fill="x",
        padx=10,
        pady=5
    )

    total_today = ttk.Label(
        total_frame,
        text="Сегодня: --",
        font=(
            "Segoe UI",
            13,
            "bold"
        )
    )

    total_today.pack(
        anchor="w",
        padx=10,
        pady=(5, 2)
    )

    total_month = ttk.Label(
        total_frame,
        text="За месяц: --",
        font=(
            "Segoe UI",
            13,
            "bold"
        )
    )

    total_month.pack(
        anchor="w",
        padx=10,
        pady=(0, 8)
    )

    return {
        "frame": meter,
        "hot": hot,
        "cold": cold,
        "single": single,
        "total_frame": total_frame,
        "total_today": total_today,
        "total_month": total_month
    }


# =========================================================
# ПОКАЗАТЬ / СКРЫТЬ БЛОК
# =========================================================

def set_block_visible(
    block,
    visible
):

    if visible:

        block["frame"].pack(
            fill="x",
            padx=10,
            pady=5
        )

    else:

        block["frame"].pack_forget()


# =========================================================
# ОБНОВЛЕНИЕ СЧЁТЧИКА
# =========================================================

def update_meter(
    meter,
    serial,
    data
):

    hot_rows = data.get(
        (serial, 1),
        []
    )

    cold_rows = data.get(
        (serial, 2),
        []
    )

    has_hot = bool(hot_rows)
    has_cold = bool(cold_rows)

    # -----------------------------------------------
    # ДВУХТАРИФНЫЙ
    # -----------------------------------------------

    if has_hot and has_cold:

        set_block_visible(
            meter["hot"],
            True
        )

        set_block_visible(
            meter["cold"],
            True
        )

        set_block_visible(
            meter["single"],
            False
        )

        hot_today, hot_month = \
            update_water_block(
                hot_rows,
                meter["hot"]["labels"]
            )

        cold_today, cold_month = \
            update_water_block(
                cold_rows,
                meter["cold"]["labels"]
            )

        meter["total_frame"].pack(
            fill="x",
            padx=10,
            pady=5
        )

        meter["total_today"].config(
            text=
            f"Сегодня: "
            f"{hot_today + cold_today:.4f} м³"
        )

        meter["total_month"].config(
            text=
            f"За месяц: "
            f"{hot_month + cold_month:.4f} м³"
        )

    # -----------------------------------------------
    # ОДНОТАРИФНЫЙ
    # -----------------------------------------------

    elif has_hot or has_cold:

        rows = (
            hot_rows
            if has_hot
            else cold_rows
        )

        set_block_visible(
            meter["hot"],
            False
        )

        set_block_visible(
            meter["cold"],
            False
        )

        set_block_visible(
            meter["single"],
            True
        )

        update_water_block(
            rows,
            meter["single"]["labels"]
        )

        meter["total_frame"].pack_forget()

    # -----------------------------------------------
    # НЕТ ДАННЫХ
    # -----------------------------------------------

    else:

        set_block_visible(
            meter["hot"],
            False
        )

        set_block_visible(
            meter["cold"],
            False
        )

        set_block_visible(
            meter["single"],
            False
        )

        meter["total_frame"].pack_forget()


# =========================================================
# ОБНОВЛЕНИЕ
# =========================================================

def update_view():

    data = read_csv()

    serials = sorted(
        set(
            serial
            for serial, tariff
            in data.keys()
        ),
        key=lambda x:
            int(x)
            if x.isdigit()
            else x
    )

    for serial in serials:

        if serial not in meters:

            meters[serial] = create_meter(
                content,
                serial
            )

    for serial, meter in meters.items():

        update_meter(
            meter,
            serial,
            data
        )

    if not serials:

        if not waiting_label.winfo_ismapped():

            waiting_label.pack(
                pady=30
            )

    else:

        if waiting_label.winfo_ismapped():

            waiting_label.pack_forget()

    root.after(
        UPDATE_MS,
        update_view
    )


# =========================================================
# ПРИМЕНИТЬ ДАТУ
# =========================================================

def apply_start_date():

    start_date = get_start_date()

    if start_date is None:

        messagebox.showerror(
            "Ошибка",
            "Введите дату в формате ДД.ММ.ГГГГ"
        )

        return

    today = datetime.now().date()

    if start_date > today:

        messagebox.showerror(
            "Ошибка",
            "Дата начала не может быть позже сегодняшней даты."
        )

        return

    # Просто обновляем расчёт.
    # CSV и BLE не трогаем.
    update_view()


# =========================================================
# ОКНО
# =========================================================

root = tk.Tk()

root.title(
    "СВТ-15 Монитор"
)

root.geometry(
    "650x760"
)

root.minsize(
    500,
    450
)


style = ttk.Style()

try:

    style.configure(
        "TLabel",
        font=(
            "Segoe UI",
            11
        )
    )

    style.configure(
        "TLabelframe.Label",
        font=(
            "Segoe UI",
            11,
            "bold"
        )
    )

except Exception:
    pass


# =========================================================
# ПАНЕЛЬ НАСТРОЕК
# =========================================================

settings = ttk.Frame(
    root
)

settings.pack(
    fill="x",
    padx=10,
    pady=10
)

ttk.Label(
    settings,
    text="Дата начала расчёта месяца:"
).pack(
    side="left"
)


start_date_var = tk.StringVar(
    value=datetime.now().strftime(
        "%d.%m.%Y"
    )
)

start_date_entry = ttk.Entry(
    settings,
    textvariable=start_date_var,
    width=12
)

start_date_entry.pack(
    side="left",
    padx=(8, 5)
)


ttk.Button(
    settings,
    text="Применить",
    command=apply_start_date
).pack(
    side="left"
)


# =========================================================
# ПРОКРУТКА
# =========================================================

canvas = tk.Canvas(
    root
)

scrollbar = ttk.Scrollbar(
    root,
    orient="vertical",
    command=canvas.yview
)

content = ttk.Frame(
    canvas
)

content.bind(
    "<Configure>",
    lambda e:
        canvas.configure(
            scrollregion=
            canvas.bbox("all")
        )
)

canvas_window = canvas.create_window(
    (0, 0),
    window=content,
    anchor="nw"
)

canvas.configure(
    yscrollcommand=scrollbar.set
)


def resize_content(event):

    canvas.itemconfig(
        canvas_window,
        width=event.width
    )


canvas.bind(
    "<Configure>",
    resize_content
)

canvas.pack(
    side="left",
    fill="both",
    expand=True
)

scrollbar.pack(
    side="right",
    fill="y"
)


# =========================================================
# СЧЁТЧИКИ
# =========================================================

meters = {}

waiting_label = ttk.Label(
    content,
    text="Ожидание данных от счётчика...",
    font=(
        "Segoe UI",
        14
    )
)


# =========================================================
# ЗАПУСК
# =========================================================

update_view()

root.mainloop()