from flask import Flask, Response
from pydexcom import Dexcom
import time
import logging
import os

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

app = Flask(__name__)

# Use environment variables for credentials (fallback to empty strings for backward compatibility)
username = os.getenv('DEXCOM_USERNAME', '')
password = os.getenv('DEXCOM_PASSWORD', '')

if not username or not password:
    logger.warning("Dexcom credentials not found in environment variables. Please set DEXCOM_USERNAME and DEXCOM_PASSWORD.")

dexcom = Dexcom(username=username, password=password)
saved_time = 0


@app.route("/")
def return_data():
    global saved_time
    seconds_elapsed = time.time() - saved_time

    glucose_reading = None
    
    if seconds_elapsed > 58:
        try:
            logger.info(f"Fetching glucose reading (last fetch was {seconds_elapsed:.1f}s ago)")
            glucose_reading = dexcom.get_current_glucose_reading()
            saved_time = time.time()  # Update saved_time after successful reading
            logger.info("Successfully fetched glucose reading")
        except Exception as e:
            logger.error(f"Failed to fetch glucose reading: {str(e)}")
            glucose_reading = None
    else:
        logger.debug(f"Skipping fetch, last reading was {seconds_elapsed:.1f}s ago")

    # Use getattr as optional chaining: returns None if glucose_reading is None or attr missing
    value = getattr(glucose_reading, "value", None)
    trend = getattr(glucose_reading, "trend_description", None)
    timestamp = getattr(glucose_reading, "datetime", None)

    if value is None and trend is None and timestamp is None:
        # No useful reading found
        logger.warning("No glucose data available")
        return Response("No Data", mimetype="text/plain", status=503)

    # Build response lines; replace missing fields with an empty string or a placeholder
    lines = [
        str(value) if value is not None else "",
        str(trend) if trend is not None else "",
        str(timestamp) if timestamp is not None else "",
    ]
    body = "\n".join(lines)

    logger.info(f"Returning glucose data: {value} {trend}")
    # Return plain text response
    return Response(body, mimetype="text/plain")


@app.route("/health")
def health_check():
    """Simple health check endpoint"""
    return Response("OK", mimetype="text/plain")


if __name__ == "__main__":
    logger.info("Starting Dexcom data server...")
    app.run(host="0.0.0.0", port=5000, debug=False)
