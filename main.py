from flask import Flask, render_template
app = Flask(__name__)
import ee

@app.route("/")
def startup():
    service_account = 'cem-ee-utility@cem-ee-utility.iam.gserviceaccount.com'    
    # credentials = ee.ServiceAccountCredentials(service_account, 'private_key.json')
    # ee.Initialize(credentials)
    return render_template("home.html")

if __name__ == "__main__":
    app.run(host="localhost", port=8080)