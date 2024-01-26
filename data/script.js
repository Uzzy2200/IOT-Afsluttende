window.addEventListener('load', getReadings);

// Create Temperature Chart
var chartT = new Highcharts.Chart({
  chart:{
    renderTo:'chart-temperature'
  },
  series: [
    {
      name: 'Temperature #1',
      type: 'line',
      color: '#101D42',
      marker: {
        symbol: 'circle',
        radius: 3,
        fillColor: '#101D42',
      }
    },
   
  ],
  title: {
    text: undefined
  },
  xAxis: {
    type: 'datetime',
    dateTimeLabelFormats: { second: '%H:%M:%S' }
    
  },
  
  yAxis: {
    title: {
      text: 'Temperature Celsius Degrees'
    }
  },
  credits: {
    enabled: false
  }
});


//Plot temperature in the temperature chart
function plotTemperature(jsonValue) {

  var keys = Object.keys(jsonValue);
  console.log(keys);
  console.log(keys.length);

  for (var i = 0; i < keys.length; i++){
    var x = (new Date()).getTime();
    console.log(x);
    const key = keys[i];
    var y = Number(jsonValue[key]);
    console.log(y);

    if(chartT.series[i].data.length > 40) {
      chartT.series[i].addPoint([x, y], true, true, true);
    } else {
      chartT.series[i].addPoint([x, y], true, false, true);
    }

  }
}

// Function to get current readings on the webpage when it loads for the first time
function getReadings(){
  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      var myObj = JSON.parse(this.responseText);
      console.log(myObj);
      plotTemperature(myObj);
    }
  };
  xhr.open("GET", "/readings", true);
  xhr.send();
}

if (!!window.EventSource) {
  var source = new EventSource('/events');

  source.addEventListener('open', function(e) {
    console.log("Events Connected");
  }, false);

  source.addEventListener('error', function(e) {
    if (e.target.readyState != EventSource.OPEN) {
      console.log("Events Disconnected");
    }
  }, false);

  source.addEventListener('message', function(e) {
    console.log("message", e.data);
  }, false);

  source.addEventListener('new_readings', function(e) {
    console.log("new_readings", e.data);
    var myObj = JSON.parse(e.data);
    console.log(myObj);
    plotTemperature(myObj);
  }, false);
}


//menu
function myFunction() {
  var x = document.getElementById("myLinks");
  if (x.style.display === "block") {
    x.style.display = "none";
  } else {
    x.style.display = "block";
  }
}

// Function to clear WiFi configuration
function clearWiFiConfig() {
  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function () {
    if (this.readyState == 4 && this.status == 200) {
      alert(this.responseText);
    }
  };
  xhr.open("GET", "/clearWiFiConfig", true);
  xhr.send();
}

function downloadCSV() {
  // Get the chart data
  var chartData = chartT.series[0].data;

  if (chartData.length > 0) {
     // Build CSV header
     var csvContent = "Timestamp, Temperature (Celsius)\n";

     // Build CSV rows
     for (var i = 0; i < chartData.length; i++) {
        var timestamp = Highcharts.dateFormat('%Y-%m-%d %H:%M:%S', chartData[i].x);
        var temperature = chartData[i].y;
        csvContent += timestamp + ',' + temperature + '\n';
     }

     // Create a Blob with the CSV data
     var blob = new Blob([csvContent], { type: 'text/csv' });

     // Create a link to download the Blob
     var link = document.createElement('a');
     link.href = window.URL.createObjectURL(blob);
     link.download = 'temperature_data.csv';

     // Append the link to the document
     document.body.appendChild(link);

     // Trigger the click event to start the download
     link.click();

     // Remove the link from the document
     document.body.removeChild(link);
  } else {
     alert("No chart data available.");
  }
}