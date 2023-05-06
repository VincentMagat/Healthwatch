from flask import Flask
from flask import request

app = Flask(__name__)
@app.route("/")
def index():
    beatsPerMinute = request.args.get("beatsPerMinute")
    temperature = request.args.get("temperature")
    print("beatsPerMinute ",beatsPerMinute)
    print("Temperature:", temperature)
    return "beatsPerMinute: " + str(beatsPerMinute) + "Temperature: " + str(temperature)
if __name__ == "__main__":
    app.run()
