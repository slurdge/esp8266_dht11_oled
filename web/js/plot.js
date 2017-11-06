var labels=[];
var temperature=[];
var humidity=[];
for (var i = data.length - 1; i >= 0; i--) {
	labels[i] = moment(data[i][0], "X").utc().format();
	temperature[i] = data[i][1];
	humidity[i] = data[i][2];
}

var trace_t = {
	type: "scatter",
	mode: "lines+markers",
	name: "Temperature",
	x: labels,
	y: temperature,
};

var trace_h = {
	type: "scatter",
	mode: "lines+markers",
	name: "Humidity",
	x: labels,
	y: humidity,
	yaxis: "y2",
};

var options = {
	yaxis: {
  	title: "Temperature (°C)",
      range: [0, 50],
		  autorange: false
	},
	yaxis2: {
		title: "Humidity (%)",
		overlaying: "y",
		side: "right",
		range: [0, 100],
		autorange: false
	},
	margin: { t: 0 }
};

Plotly.plot( "chart-plotly", [trace_t,trace_h], options );