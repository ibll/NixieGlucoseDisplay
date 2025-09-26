from flask import Flask, Response
from pydexcom import Dexcom
import time

app = Flask(__name__)
dexcom = Dexcom(username="", password="")

saved_time = 0


@app.route("/")
def return_data():
    global saved_time
    seconds_elapsed = time.time() - saved_time
    glucose_reading = None

    if seconds_elapsed > 58:
        saved_time = time.time()
        try:
            glucose_reading = dexcom.get_current_glucose_reading()
        except Exception:
            pass

    # Use getattr as optional chaining: returns None if glucose_reading is None or attr missing
    value = getattr(glucose_reading, "value", None)
    trend = getattr(glucose_reading, "trend_description", None)
    timestamp = getattr(glucose_reading, "datetime", None)

    if value is None and trend is None and timestamp is None:
        # No useful reading found
        return "No Data"

    # Build response lines; replace missing fields with an empty string or a placeholder
    lines = [
        str(value) if value is not None else "",
        str(trend) if trend is not None else "",
        str(timestamp) if timestamp is not None else "",
    ]
    body = "\n".join(lines)

    # Return plain text response
    return Response(body, mimetype="text/plain")


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)
