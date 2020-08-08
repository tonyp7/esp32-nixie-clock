var colorPicker = null;
var sleepMode = null;
var selectedItem = -1;

const gel = (e) => document.getElementById(e);
const zeroPad = (num, places) => String(num).padStart(places, '0');
const week = ["Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"];
const MAX_SLEEPMODES = 4;


function truncate (num, places) {
	return Math.trunc(num * Math.pow(10, places)) / Math.pow(10, places);
}

function docReady(fn) {
	// see if DOM is already available
	if (
		document.readyState === "complete" ||
		document.readyState === "interactive"
	) {
		// call on next available tick
		setTimeout(fn, 1);
	} 
	else {
		document.addEventListener("DOMContentLoaded", fn);
	}
}



function buildDayHTML(day){

	let ret = "";
	let bit = 0x01;
	for (let i = 0; i < 7; i++) {

		if( (day & bit) > 0){
			ret += "<b>" + week[i] + "</b>";
		}
		else{
			ret += "<i>" + week[i] + "</i>";
		}

		if(i!=6) ret += " ";

		bit <<= 1;
	}

	return ret;
}


function timeFromStr(str){

	var a = str.split(":");
	var hours = parseInt(a[0], 10);
	var minutes = parseInt(a[1], 10);
	
	return Math.round(  hours * 3600 + minutes * 60  );
}

function buildTime(t){

	let hours =  truncate(t / 3600, 0);
	let minutes = Math.round((t % 3600) / 60);

	return zeroPad(hours.toString(), 2) + ":" + zeroPad(minutes.toString(), 2);
}


function colorChangeCallback(color) {

	console.log(color.rgb);

	try{
		res = fetch("backlights/", {
			method: "POST",
			headers: {
			  "Content-Type": "application/json",
			},
			body: JSON.stringify(color.rgb),
		  });
	}
	catch (e) {
		console.info("error in colorChangeCallback");
	}
	
}

async function getTimezones(){

	try{

		let restz = await fetch("timezone/");
		let timezone = await restz.text();

		let res = await fetch("timezones.json");
		let timezones = await res.json();

		let sel = gel("timezone-select");
		sel.innerHTML = "";

		timezones.forEach(function(entry) {
			let opt = document.createElement('option');
			opt.value = entry;
			opt.selected = entry == timezone;
			opt.innerHTML = entry;
			sel.appendChild(opt);
		});


	}
	catch (e){
		console.info("error" + e);
	}

}

async function getSleepMode(url = "sleepmode/") {
	try {


		var res;
		
		if(sleepMode == null){
			res = await fetch(url);
		}
		else{
			res = await fetch(url, {
				method: "POST",
				headers: {
				  "Content-Type": "application/json",
				},
				body: JSON.stringify(sleepMode),
			  });
		}
		var mode = await res.json();

		//create list of sleep modes

		let chkbx = gel("sleepmode-enable");
		chkbx.checked = mode.enabled;
		if(mode.enabled){

			let activeSM = 0;
			for (let i = 0; i < mode.data.length; i++) {
				let sleepItem = mode.data[i];
				let domSleepItem = gel("sm" + i.toString());

				if(sleepItem.enabled){
					activeSM++;
					domSleepItem.style.display = 'block';

					let d = buildDayHTML(sleepItem.days);
					let from = buildTime(sleepItem.from);
					let to = buildTime(sleepItem.to);

					domSleepItem.innerHTML = d + ", <b>" + from + "</b> to <b>" + to + "</b>";
				}
				else{
					domSleepItem.style.display = 'none';
				}
			}

			//hide or show new button
			let newSM = gel("sm" + MAX_SLEEPMODES.toString());
			if(activeSM == MAX_SLEEPMODES){
				newSM.style.display = 'none';
			}
			else{
				newSM.style.display = 'block';
			}

			//show the sleepmodes
			gel("sleepmodes").style.display = 'block';
		}
		else{
			//hide the whole thing
			gel("sleepmodes").style.display = 'none';
		}



		sleepMode = mode;
		
	} 
	catch (e) {
		console.info("cannot access sleepmode");
	}
}


async function sleepModeDel(){

	if(selectedItem >= 0){


		let data = sleepMode.data[selectedItem];

		data.enabled = false;
		data.days = 0;
		data.from = 0;
		data.to = 0;

		sleepMode.data.splice(selectedItem, 1);
		sleepMode.data.push(data);

		await getSleepMode();
	}
	gel("diag-sleepmode").style.display = "none";
	gel("clock-wrap").classList.remove("blur");
	

}

async function changeTimezone(){
	
	let sel = gel("timezone-select");
	let data = { "timezone": sel.value};

	console.log(data);

	try{
		let res = await fetch("timezone/", {
			method: "POST",
			headers: {
			  "Content-Type": "application/json",
			},
			body: JSON.stringify(data),
		  });
	}
	catch (e) {
		console.info("error in changeTimezone");
	}

}

async function sleepModeOk(){

	if(selectedItem >= 0){

		let data = sleepMode.data[selectedItem];

		//build days bitfield based on checkboxes
		let days = 0;
		let bit = 0x01;
		for (let i = 0; i < 7; i++) {
			let chkbx = gel(week[i]);
			if(chkbx.checked){
				days |= bit;
			}
			bit <<= 1;
		}

		//convert time strings to seconds
		let tf = gel("time-from");
		let tt = gel("time-to");
		let from = timeFromStr(tf.value);
		let to = timeFromStr(tt.value);

		data.enabled =  (days != 0) && ( from != to);

		if(data.enabled){
			data.days = days;
			data.from = from;
			data.to = to;
		}
		else{
			//invalid entry
			data.days = 0;
			data.from = 0;
			data.to = 0;
		}

		await getSleepMode();

	}
	gel("diag-sleepmode").style.display = "none";
	gel("clock-wrap").classList.remove("blur");
}

function changeSleepMode(e){

	//fill in data
	let id = parseInt(e.currentTarget.id.charAt(2));
	let sleepItem = null;
	if(id == MAX_SLEEPMODES){

		//new sleepmode: -- find the first empty one
		for(let i=0;i<MAX_SLEEPMODES;i++){
			if( !sleepMode.data[i].enabled ){
				sleepItem = sleepMode.data[i];

				sleepItem.enabled = false;
				sleepItem.days = 0;
				sleepItem.from = 0;
				sleepItem.to = 0;

				id = i;

				break;
			}
		}

		//hide delete button
		gel("del-sleepmode").style.display = "none";
	}
	else{
		sleepItem = sleepMode.data[id];
		//show delete button
		gel("del-sleepmode").style.display = "block";
	}


	if(sleepItem != null){
		//save selected item for when user clicks delete or ok
		selectedItem = id;

		//build checkboxes
		let bit = 0x01;
		for (let i = 0; i < 7; i++) {
			let chkbx = gel(week[i]);
			chkbx.checked = (sleepItem.days & bit) > 0
			bit <<= 1;
		}

		//build times
		let tf = gel("time-from");
		let tt = gel("time-to");
		tf.value = buildTime(sleepItem.from);
		tt.value = buildTime(sleepItem.to);

		gel("diag-sleepmode").style.display = "block";
		gel("clock-wrap").classList.add("blur");
	}
}

docReady(async function () {
	console.log("ready!");

	await getSleepMode();
	await getTimezones();

	var colorPicker = new iro.ColorPicker('#color-picker-container');
	colorPicker.on("color:change", colorChangeCallback);

	for(let i=0; i < MAX_SLEEPMODES+1; i++){
		gel("sm" + i.toString()).addEventListener(
		"click",
			(e) => {
				changeSleepMode(e);
			},
			false
		);
	}



	gel("sleepmode-enable").addEventListener(
	"click",
		async (e) => {
			
			let chkd = gel("sleepmode-enable").checked;

			if(sleepMode != null){

				if(chkd != sleepMode.enabled){
					sleepMode.enabled = chkd;
					await getSleepMode();
				}
			}
		},
		false
	);


	gel("timezone-select").addEventListener('change', async (event) => {
		await changeTimezone();
	});

	gel("cancel-sleepmode").addEventListener(
	"click",
		(e) => {
			gel("diag-sleepmode").style.display = "none";
			gel("clock-wrap").classList.remove("blur");
		},
		false
	);

	gel("del-sleepmode").addEventListener(
	"click",
		(e) => {
			sleepModeDel();
		},
		false
	);

	gel("ok-sleepmode").addEventListener(
	"click",
		(e) => {
			sleepModeOk();
		},
		false
	);
});



    