/**
 * Add gobals here
 */
let seconds = null;
let otaTimerVar = null;
let wifiConnectInterval;

/**
 * Initialize functions here.
 */
$(document).ready(function () {
  getUpdateStatus();
  getConnectInfo();
  startAM2320Interval();
  $("#connect_wifi").on("click", () => {
    checkCredentials();
  });

  $("#disconnect_wifi").on("click", function () {
    disconnectWifi();
  });
});

/**
 * Gets file name and size for display on the web page.
 */
function getFileInfo() {
  var x = document.getElementById("selected_file");
  var file = x.files[0];

  document.getElementById("file_info").innerHTML =
    "<h4>File: " + file.name + "<br>" + "Size: " + file.size + " bytes</h4>";
}

/**
 * Handles the firmware update.
 */
function updateFirmware() {
  // Form Data
  var formData = new FormData();
  var fileSelect = document.getElementById("selected_file");

  if (fileSelect.files && fileSelect.files.length == 1) {
    var file = fileSelect.files[0];
    formData.set("file", file, file.name);
    document.getElementById("ota_update_status").innerHTML =
      "Uploading " + file.name + ", Firmware Update in Progress...";

    // Http Request
    var request = new XMLHttpRequest();

    request.upload.addEventListener("progress", updateProgress);
    request.open("POST", "/OTAupdate");
    request.responseType = "blob";
    request.send(formData);
  } else {
    window.alert("Select A File First");
  }
}

/**
 * Progress on transfers from the server to the client (downloads).
 */
function updateProgress(oEvent) {
  if (oEvent.lengthComputable) {
    getUpdateStatus();
  } else {
    window.alert("total size is unknown");
  }
}

/**
 * Posts the firmware udpate status.
 */
function getUpdateStatus() {
  var xhr = new XMLHttpRequest();
  var requestURL = "/OTAstatus";
  xhr.open("POST", requestURL, false);
  xhr.send("ota_update_status");

  if (xhr.readyState == 4 && xhr.status == 200) {
    var response = JSON.parse(xhr.responseText);

    document.getElementById("latest_firmware").innerHTML =
      response.compile_date + " - " + response.compile_time;

    // If flashing was complete it will return a 1, else -1
    // A return of 0 is just for information on the Latest Firmware request
    if (response.ota_update_status == 1) {
      // Set the countdown timer time
      seconds = 10;
      // Start the countdown timer
      otaRebootTimer();
    } else if (response.ota_update_status == -1) {
      document.getElementById("ota_update_status").innerHTML =
        "!!! Upload Error !!!";
    }
  }
}

/**
 * Displays the reboot countdown.
 */
function otaRebootTimer() {
  document.getElementById("ota_update_status").innerHTML =
    "OTA Firmware Update Complete. This page will close shortly, Rebooting in: " +
    seconds;

  if (--seconds == 0) {
    clearTimeout(otaTimerVar);
    window.location.reload();
  } else {
    otaTimerVar = setTimeout(otaRebootTimer, 1000);
  }
}

/**
 * Get AM2320 readings for display.
 */
function getAM2320SensorValues() {
  $.getJSON("/am2320Sensor.json", function (data) {
    $("#temprature_reading").text(data["temp"]);
    $("#humidity_reading").text(data["humidity"]);
  });
}

function startAM2320Interval() {
  setInterval(() => {
    getAM2320SensorValues();
  }, 5000);
}

function stopWifiConnectStatusInterval() {
  wifiConnectInterval && clearInterval(wifiConnectInterval);
}

function getWiFiConnectStatus() {
  let xhr = new XMLHttpRequest();
  let requestURL = "/wifiConnectStatus.json";
  xhr.open("POST", requestURL, false);
  xhr.send("wifi_connect_status");

  if (xhr.readyState == 4 && xhr.status == 200) {
    let response = JSON.parse(xhr.responseText);

    document.getElementById("wifi_connect_status").innerHTML = "Connecting...";

    if (response.wifi_connect_status == 2) {
      document.getElementById("wifi_connect_status").innerHTML =
        "<h4 class='red'>Failed to connect. Please check credentials.</h4>";

      stopWifiConnectStatusInterval();
    } else if (response.wifi_connect_status == 3) {
      document.getElementById("wifi_connect_status").innerHTML =
        "<h4 class='green'>Connected.</h4>";

      stopWifiConnectStatusInterval();
      getConnectInfo();
    }
  }
}

function startWiFiConnectStatusInterval() {
  wifiConnectInterval = setInterval(getWiFiConnectStatus, 1000);
}

function connectWiFi() {
  let selectedSSID = $("#connect_ssid").val();
  let pwd = $("#connect_pass").val();

  $.ajax({
    url: "/wifiConnect.json",
    dataType: "json",
    method: "POST",
    cache: false,
    headers: { "my-connect-ssid": selectedSSID, "my-connect-pass": pwd },
    data: { timestamp: Date.now() },
  });

  startWiFiConnectStatusInterval();
}

function checkCredentials() {
  let errorList = "";
  let credsOk = true;

  let selectedSSID = $("#connect_ssid").val();
  let pwd = $("#connect_pass").val();

  if (selectedSSID == "") {
    errorList += "<h4 class='red'>SSID cannot be empty</h4>";
    credsOk = false;
  }

  if (pwd == "") {
    errorList += "<h4 class='red'>Password cannot be empty</h4>";
    credsOk = false;
  }

  if (!credsOk) {
    $("#wifi_connect_credentials_error").html(errorList);
  } else {
    $("#wifi_connect_credentials_error").html("");
    connectWiFi();
  }
}

function showpassword() {
  const passElement = document.getElementById("connect_pass");

  if (passElement.type === "password") {
    passElement.type = "text";
  } else {
    passElement.type = "password";
  }
}

/**
 * Gets the connection information for displaying on the web page.
 */
function getConnectInfo() {
  $.getJSON("/wifiConnectInfo.json", function (data) {
    $("#connected_ap_label").html("Connected to: ");
    $("#connected_ap").text(data["ap"]);

    $("#ip_address_label").html("IP Address: ");
    $("#wifi_connect_ip").text(data["ip"]);

    $("#netmask_label").html("Netmask: ");
    $("#wifi_connect_netmask").text(data["netmask"]);

    $("#gateway_label").html("Gateway: ");
    $("#wifi_connect_gw").text(data["gw"]);

    document.getElementById("disconnect_wifi").style.display = "block";
  });
}

/**
 * Disconnects Wifi once the disconnect button is pressed and reloads the web page.
 */
function disconnectWifi() {
  $.ajax({
    url: "/wifiDisconnect.json",
    dataType: "json",
    method: "DELETE",
    cache: false,
    data: { timestamp: Date.now() },
  });
  // Update the web page
  setTimeout("location.reload(true);", 2000);
}

/**
 * Sets the interval for displaying local time.
 */
function startLocalTimeInterval() {
  setInterval(getLocalTime, 10000);
}

/**
 * Gets the local time.
 * @note connect the ESP32 to the internet and the time will be updated.
 */
function getLocalTime() {
  $.getJSON("/localTime.json", function (data) {
    $("#local_time").text(data["time"]);
  });
}

/**
 * Gets the ESP32's access point SSID for displaying on the web page.
 */
function getSSID() {
  $.getJSON("/apSSID.json", function (data) {
    $("#ap_ssid").text(data["ssid"]);
  });
}

function disconnectWifi() {
  $.ajax({
    url: "/wifiDisconnect.json",
    dataType: "json",
    method: "DELETE",
    cache: false,
    data: { timestamp: Date.now() },
  });
  // Update the web page
  setTimeout("location.reload(true);", 2000);
}
