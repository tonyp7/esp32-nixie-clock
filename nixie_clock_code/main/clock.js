var colorPicker = null;

function colorChangeCallback(color) {
  console.log(color.rgb.r);
  
  	$.ajax({
		url: '/color',
		dataType: 'json',
		method: 'POST',
		cache: false,
		headers: { 'X-Custom-R' : color.rgb.r, 'X-Custom-G' : color.rgb.g, 'X-Custom-B' : color.rgb.b }
		//data: { 'timestamp': Date.now()}
	});
}

$(document).ready(function(){
	
	
	$.getJSON("https://api.mclk.org/timezones", function(result) {
		var $dropdown = $("#timezone-select");
		$.each(result, function() {
			
			$dropdown.append($('<option>', {value:1, text:this}));
			//alert(this);
			//$dropdown.append($("<option />").val(this);
		});
	})
	  .fail(function( jqxhr, textStatus, error ) {
	});
	
	
	
	colorPicker.on("color:change", colorChangeCallback);
	
	

	
});



    