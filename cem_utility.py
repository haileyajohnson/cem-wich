from flask import Flask, render_template
app = Flask(__name__, static_folder="_dist")

import ee
import os

@app.route("/")
def startup():
    # service_account = 'cem-ee-utility@cem-ee-utility.iam.gserviceaccount.com'    
    # credentials = ee.ServiceAccountCredentials(service_account, 'private_key.json')
    # ee.Initialize(credentials)
    distDir = []
    for filename in os.listdir('_dist'):
        if filename.endswith(".js"):
            distDir.append(filename)
        else:
            continue
    return render_template("application.html", distDir=distDir)

if __name__ == "__main__":
    app.run(host="localhost", port=8080)