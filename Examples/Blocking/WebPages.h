/////////////////////////////////////////////////////////////////////////////////
// WebPages.h
//
// Contains the (very long) string that represents the root web page for
// timezone / DST configuration using WiFiManager.
//
// History:
// - jmcorbett 07-SEP-2022 Original creation.
//
// Copyright (c) 2022, Joseph M. Corbett
/////////////////////////////////////////////////////////////////////////////////
#if !defined WEBPAGES_H
#define WEBPAGES_H

const char TZ_SELECT_STR[] = R"=====(
    <!-- TIMEZONE SELECTION -->
    <br/>
    <h3 style="display:inline">TIMEZONE:</h3>
    <select name="timezoneOffset" id="timezoneOffset">
      <option value="-720">(GMT -12:00) Eniwetok, Kwajalein</option>
      <option value="-660">(GMT -11:00) Midway Island, Samoa</option>
      <option value="-600">(GMT -10:00) Hawaii</option>
      <option value="-570">(GMT -9:30) Taiohae</option>
      <option value="-540">(GMT -9:00) Alaska</option>
      <option value="-480">(GMT -8:00) Pacific Time (US &amp; Canada)</option>
      <option value="-420">(GMT -7:00) Mountain Time (US &amp; Canada)</option>
      <option value="-360">(GMT -6:00) Central Time (US &amp; Canada), Mexico City</option>
      <option value="-300">(GMT -5:00) Eastern Time (US &amp; Canada), Bogota, Lima</option>
      <option value="-270">(GMT -4:30) Caracas</option>
      <option value="-240">(GMT -4:00) Atlantic Time (Canada), Caracas, La Paz</option>
      <option value="-210">(GMT -3:30) Newfoundland</option>
      <option value="-180">(GMT -3:00) Brazil, Buenos Aires, Georgetown</option>
      <option value="-120">(GMT -2:00) Mid-Atlantic</option>
      <option value="-60">(GMT -1:00) Azores, Cape Verde Islands</option>
      <option value="0">(GMT) Western Europe Time, London, Lisbon, Casablanca</option>
      <option value="60">(GMT +1:00) Brussels, Copenhagen, Madrid, Paris</option>
      <option value="120">(GMT +2:00) Kaliningrad, South Africa</option>
      <option value="180">(GMT +3:00) Baghdad, Riyadh, Moscow, St. Petersburg</option>
      <option value="210">(GMT +3:30) Tehran</option>
      <option value="240">(GMT +4:00) Abu Dhabi, Muscat, Baku, Tbilisi</option>
      <option value="270">(GMT +4:30) Kabul</option>
      <option value="300">(GMT +5:00) Ekaterinburg, Islamabad, Karachi, Tashkent</option>
      <option value="330">(GMT +5:30) Bombay, Calcutta, Madras, New Delhi</option>
      <option value="345">(GMT +5:45) Kathmandu, Pokhara</option>
      <option value="360">(GMT +6:00) Almaty, Dhaka, Colombo</option>
      <option value="390">(GMT +6:30) Yangon, Mandalay</option>
      <option value="420">(GMT +7:00) Bangkok, Hanoi, Jakarta</option>
      <option value="480">(GMT +8:00) Beijing, Perth, Singapore, Hong Kong</option>
      <option value="525">(GMT +8:45) Eucla</option>
      <option value="540">(GMT +9:00) Tokyo, Seoul, Osaka, Sapporo, Yakutsk</option>
      <option value="570">(GMT +9:30) Adelaide, Darwin</option>
      <option value="600">(GMT +10:00) Eastern Australia, Guam, Vladivostok</option>
      <option value="630">(GMT +10:30) Lord Howe Island</option>
      <option value="660">(GMT +11:00) Magadan, Solomon Islands, New Caledonia</option>
      <option value="690">(GMT +11:30) Norfolk Island</option>
      <option value="720">(GMT +12:00) Auckland, Wellington, Fiji, Kamchatka</option>
      <option value="765">(GMT +12:45) Chatham Islands</option>
      <option value="780">(GMT +13:00) Apia, Nukualofa</option>
      <option value="840">(GMT +14:00) Line Islands, Tokelau</option>
    </select>
    <!-- DST END ABBREVIATION SELECTION -->
    <h3 style="display:inline">TIMEZONE ABBREVIATION:</h3>
    <input type="text" id="dstEndString" name="dstEndString" maxlength="5">
    <!-- USE DST CHECKBOX -->
    <br><br>
    <input type="checkbox" id="useDstField" name="useDstField" value="true"  onchange="checkUseDst()">
    <h3 style="display:inline"><label for="useDstField">&nbsp Use DST</label></h3>
    <br class="canHide"><br class="canHide">
    <!-- DST START ABBREVIATION SELECTION -->
    <h3 class="canHide">DST ABBREVIATION:</h3><br>
    <input type="text" id="dstStartString" name="dstStartString" class="canHide" maxlength="5">
    <br class="canHide"><br class="canHide">
    <h3 class="canHide">DST STARTS ON:</h3><br>
    <!-- DST START WEEK NUMBER SELECTION -->
    <select name="weekNumber1" id="weekNumber1" class="canHide">
      <option value="1">First</option>
      <option value="2">Second</option>
      <option value="3">Third</option>
      <option value="4">Fourth</option>
      <option value="0">Last</option>
    </select>
    <!-- DST START DAY OF WEEK SELECTION -->
    <select name="dayOfWeek1" id="dayOfWeek1" class="canHide">
      <option value="1">Sunday</option>
      <option value="2">Monday</option>
      <option value="3">Tuesday</option>
      <option value="4">Wednesday</option>
      <option value="5">Thursday</option>
      <option value="6">Friday</option>
      <option value="7">Saturday</option>
    </select>
    <!-- DST START MONTH SELECTION -->
    <select name="month1" id="month1" class="canHide">
      <option value="1">of January</option>
      <option value="2">of February</option>
      <option value="3">of March</option>
      <option value="4">of April</option>
      <option value="5">of May</option>
      <option value="6">of June</option>
      <option value="7">of July</option>
      <option value="8">of August</option>
      <option value="9">of September</option>
      <option value="10">of October</option>
      <option value="11">of November</option>
      <option value="12">of December</option>
    </select>
    <!-- DST START HOUR SELECTION -->
    <select name="hour1" id="hour1" class="canHide">
      <option value="0">at Midnight</option>
      <option value="1">at 1:00 AM</option>
      <option value="2">at 2:00 AM</option>
      <option value="3">at 3:00 AM</option>
      <option value="4">at 4:00 AM</option>
      <option value="5">at 5:00 AM</option>
      <option value="6">at 6:00 AM</option>
      <option value="7">at 7:00 AM</option>
      <option value="8">at 8:00 AM</option>
      <option value="9">at 9:00 AM</option>
      <option value="10">at 10:00 AM</option>
      <option value="11">at 11:00 AM</option>
      <option value="12">at Noon</option>
      <option value="13">at 1:00 PM</option>
      <option value="14">at 2:00 PM</option>
      <option value="15">at 3:00 PM</option>
      <option value="16">at 4:00 PM</option>
      <option value="17">at 5:00 PM</option>
      <option value="18">at 6:00 PM</option>
      <option value="19">at 7:00 PM</option>
      <option value="20">at 8:00 PM</option>
      <option value="21">at 9:00 PM</option>
      <option value="22">at 10:00 PM</option>
      <option value="23">at 11:00 PM</option>
    </select>
    <!-- DST OFFSET SELECTION -->
    <select name="dstOffset" id="dstOffset" class="canHide">
      <option value="30">add 30 minutes</option>
      <option value="60">add 60 minutes</option>
    </select>
    <!-- DST END WEEK NUMBER SELECTION -->
    <br class="canHide"><br class="canHide">
    <h3 id="dstEnd" class="canHide">DST ENDS ON:</h3><br>
    <select name="weekNumber2" id="weekNumber2" class="canHide">
      <option value="1">First</option>
      <option value="2">Second</option>
      <option value="3">Third</option>
      <option value="4">Fourth</option>
      <option value="0">Last</option>
    </select>
    <!-- DST END DAY OF WEEK SELECTION -->
    <select name="dayOfWeek2" id="dayOfWeek2" class="canHide">
      <option value="1">Sunday</option>
      <option value="2">Monday</option>
      <option value="3">Tuesday</option>
      <option value="4">Wednesday</option>
      <option value="5">Thursday</option>
      <option value="6">Friday</option>
      <option value="7">Saturday</option>
    </select>
    <!-- DST END MONTH SELECTION -->
    <select name="month2" id="month2" class="canHide">
      <option value="1">of January</option>
      <option value="2">of February</option>
      <option value="3">of March</option>
      <option value="4">of April</option>
      <option value="5">of May</option>
      <option value="6">of June</option>
      <option value="7">of July</option>
      <option value="8">of August</option>
      <option value="9">of September</option>
      <option value="10">of October</option>
      <option value="11">of November</option>
      <option value="12">of December</option>
    </select>
    <!-- DST END HOUR SELECTION -->
    <select name="hour2" id="hour2" class="canHide">
      <option value="0">at Midnight</option>
      <option value="1">at 1:00 AM</option>
      <option value="2">at 2:00 AM</option>
      <option value="3">at 3:00 AM</option>
      <option value="4">at 4:00 AM</option>
      <option value="5">at 5:00 AM</option>
      <option value="6">at 6:00 AM</option>
      <option value="7">at 7:00 AM</option>
      <option value="8">at 8:00 AM</option>
      <option value="9">at 9:00 AM</option>
      <option value="10">at 10:00 AM</option>
      <option value="11">at 11:00 AM</option>
      <option value="12">at Noon</option>
      <option value="13">at 1:00 PM</option>
      <option value="14">at 2:00 PM</option>
      <option value="15">at 3:00 PM</option>
      <option value="16">at 4:00 PM</option>
      <option value="17">at 5:00 PM</option>
      <option value="18">at 6:00 PM</option>
      <option value="19">at 7:00 PM</option>
      <option value="20">at 8:00 PM</option>
      <option value="21">at 9:00 PM</option>
      <option value="22">at 10:00 PM</option>
      <option value="23">at 11:00 PM</option>
    </select>
    <br class="canHide">
    <!-- NTP SERVER SELECTION -->
    <br>
    <h3 style="display:inline">NTP SERVER ADDRESS:</h3>
    <input type="text" id="ntpServerAddr" name="ntpServerAddr" maxlength="25">
    <br>
    <h3 style="display:inline">NTP SERVER PORT:</h3>
    <input type="number" id="ntpServerPort" name="ntpServerPort" min="1" max="65535">
    <br>
</body>
<script>
  // Global variables.

  // Initialize globals on page load.
  window.onload = (event) => {
    // Initialize current timezone/dst settings.
    initializeSettings();

    // Hide or show the initial DST values selections.
    checkUseDst();
  }
  
  // Initialize current timezone/dst settings.
  function initializeSettings() {
    // Refresh page since we might have arrived here due returning from save.
    if (sessionStorage.getItem("doReload") == "true") { 
      sessionStorage.setItem("doReload", "false");
      location.reload();
    }
    // Find the page's submit button, and trigger a reload when it is clicked.
    var objs = document.getElementsByTagName("button");
    for (var i = 0; i < objs.length; i++) {
      if (objs[i].type = "submit") {
        objs[i].onclick = function() {sessionStorage.setItem("doReload", "true");}
        break;
      }
    }   
    
    let tzJsonData = '*PUT_TZ_JSON_DATA_HERE*';

    let json = JSON.parse(tzJsonData);
    let timeZone = json.TIMEZONE;    
    let useDst = json.USE_DST == true;
    let dstStartWeek = json.DST_START_WEEK;
    let dstStartDow = json.DST_START_DOW;
    let dstStartMonth = json.DST_START_MONTH;
    let dstStartHour = json.DST_START_HOUR;
    let dstStartOffset = json.DST_START_OFFSET;
    let dstEndWeek = json.DST_END_WEEK;
    let dstEndDow = json.DST_END_DOW;
    let dstEndMonth = json.DST_END_MONTH;
    let dstEndHour = json.DST_END_HOUR;
    let tzAbbrev = json.TZ_ABBREVIATION;
    let dstAbbrev = json.DST_ABBREVIATION;
    let ntpAddr = json.NTP_ADDRESS;
    let ntpPort = json.NTP_PORT;

    // Initialize the select fields.
    setSelectedIndex("timezoneOffset", timeZone);    
    setSelectedIndex("weekNumber1", dstStartWeek);    
    setSelectedIndex("dayOfWeek1", dstStartDow);    
    setSelectedIndex("month1", dstStartMonth);    
    setSelectedIndex("hour1", dstStartHour);    
    setSelectedIndex("dstOffset", dstStartOffset);    
    setSelectedIndex("weekNumber2", dstEndWeek);    
    setSelectedIndex("dayOfWeek2", dstEndDow);    
    setSelectedIndex("month2", dstEndMonth);    
    setSelectedIndex("hour2", dstEndHour);
    
    document.getElementById("useDstField").checked = useDst;
    document.getElementById("dstEndString").value = tzAbbrev;
    document.getElementById("dstStartString").value = dstAbbrev;
    
    document.getElementById("ntpServerAddr").value = ntpAddr;
    document.getElementById("ntpServerPort").value = ntpPort;
  }

  // Hide/unhide DST related fields based on DST checkbox.
  function checkUseDst() {
    let dstIsChecked = document.getElementById("useDstField").checked;
    let dispType = "inline";
    if (!dstIsChecked) {
      dispType = "none";
    }
    let x = document.getElementsByClassName("canHide");
    for (var i = 0; i < x.length; i++) {
      x[i].style.display = dispType;
    }
  }

  // Select an option of a selection list based on its value.
  function setSelectedIndex(s, v) {
    let obj = document.getElementById(s);
    for (let i = 0; i < obj.options.length; i++) {
      if (obj.options[i].value == v) {
        obj.options[i].selected = true;
        return;
      }
    }
  }

</script>

)=====";  // End TZ_SELECT_STR[].

#endif // WEBPAGES_H