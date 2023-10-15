let mqttClient;
const plugs = []; //Array containing the ID's of all the plugs

window.addEventListener("load", (event) => {
    connectToBroker();
});

/**
 * Connect to broker and set callbacks
 */
function connectToBroker() {

    //Give our client a random clientID
    const clientId = "client" + Math.random().toString(16).substring(2, 8);

    //Broker to connect to
    const host = "ws://192.168.1.5:1083/mqtt";

    const options = {
        keepalive: 60,
        clientId: clientId,
        protocolId: "MQTT",
        protocolVersion: 4,
        clean: true,
        reconnectPeriod: 1000,
        connectTimeout: 30 * 1000,
        resubscribe: false
    };

    mqttClient = mqtt.connect(host, options);

    //Error callback
    mqttClient.on("error", (err) => {
        console.log("Error: ", err);
        mqttClient.end();
    });

    //Reconnect callback
    mqttClient.on("reconnect", () => {
        console.log("Reconnecting to broker");
    });

    //Connect callback
    mqttClient.on("connect", () => {
        console.log(`Connected to broker ${host}`);
        subscribeToSmartPlugs();
    });

    //Message callback
    mqttClient.on("message", (topic, message) => {
        //Shelly API has the model and ID on second layer
        let plug_id = topic.split("/")[1].toString();
        //Most topics to obtain values are based on this topic
        let mainTopic = `shellies/${plug_id}/relay/0`;

        //Check if plug is already known and manage it if not
        managePlugs(plug_id);

        console.log(
            "Received Message: " + message.toString() + "\nOn topic: " + topic
        );

        //Depending on the topic the message came through, update the label in the list
        if (topic == `${mainTopic}/power`) {
            const messageTextArea = document.querySelector(`#${plug_id}-power`);
            messageTextArea.innerHTML = `Power: ${ message } W`;
        } else if (topic == `${mainTopic}/energy`) {
            const messageTextArea = document.querySelector(`#${plug_id}-energy`);
            messageTextArea.innerHTML = `Energy: ${ message } Wmin`;
        } else if (topic == `shellies/${plug_id}/temperature`) {
            const messageTextArea = document.querySelector(`#${plug_id}-temperature`);
            messageTextArea.innerHTML = `Temperature: ${ message } C`;
        } else if (topic == `shellies/${plug_id}/overtemperature`) {
            const messageTextArea = document.querySelector(`#${plug_id}-overtemperature`);
            messageTextArea.innerHTML = `Overtemperature: ${ (message == "0" ? "false" : "true") }`;
        } else if (topic == mainTopic) {
            const messageTextArea = document.querySelector(`#${plug_id}-status`);
            messageTextArea.innerHTML = `Status: ${ message }`;
            //The button should only show up once we know the status and contain the opposite of status
            const publishBtn = document.querySelector(`#${plug_id}-button`);
            publishBtn.innerHTML = `Turn plug ${(message == "on") ? "off" : "on"}`;
            publishBtn.style.display = "block";
        }
    });
}

/**
 * Subscribe callback
 * @param {*} err Error
 * @param {*} granted Array containing {topic, qos}
 */
function on_subscribe(err, granted) {
    console.log(`Subscribed to ${granted[0]["topic"]}`);
}

/**
 * Subscribe to the shellies topic
 */
function subscribeToSmartPlugs() {
    mqttClient.subscribe("shellies/+/#", { qos: 0 }, on_subscribe);
}

/**
 * Publish toggle command to shelly topic
 * @param {*} plug ID of plug
 */
function toggleRelay(plug) {
    mqttClient.publish(`shellies/${plug}/relay/0/command`, "toggle", {
        qos: 0,
        retain: false,
    });
}

/***
 * Manages plugs by checking if plug already known, if not, add to list and create HTML entries
 * @param {*} plug_id Name of plug
 */
function managePlugs(plug_id) {
    for (let index = 0; index < plugs.length; index++) {
        if (plugs[index] == plug_id) {
            console.log(`Plug with ID ${plug_id} already known`);
            return;
            //break;
        }
    }

    //Plug is not known, make sure it isn't an announcement, then add to list and create HTML entry
    if (!plug_id.includes("announce")) {
        plugs.push(plug_id);
        console.log(`Added plug with name ${plug_id}`);
        insertPlugIntoHTML(plug_id);
    }
}

/**
 * Creates a HTML div with data relative to a plug
 * @param {*} plug_id ID of plug
 */
function insertPlugIntoHTML(plug_id) {
    //Create div to just format it  nicer
    const div = document.createElement("div");
    div.id = `${plug_id}-div`;
    div.style.float = "left";
    div.style.width = "30%";
    div.style.border = "5px solid black";
    div.style.margin = "20px";
    //Main div defined in smartPlug.html
    const myDiv = document.getElementById("plugs");
    myDiv.appendChild(div);

    //Headline containing name of plug
    const headline = document.createElement("h1");
    headline.innerHTML = plug_id.toString();
    headline.style.textAlign = "center";

    //A unnumbered list containing all the data about the plug
    const list = document.createElement("ul");
    //Small function to create list entries, they are set to N/A by default (no data obtained yet)
    const createListItem = (id) => {
        const listItem = document.createElement("li");
        const label = document.createElement("label");
        label.id = `${plug_id}-${id}`;
        switch (id) {
            case "power":
                label.innerHTML = "Power: N/A";
                break;
            case "energy":
                label.innerHTML = "Energy: N/A";
                break;
            case "temperature":
                label.innerHTML = "Temperature: N/A";
                break;
            case "status":
                label.innerHTML = "Status: N/A";
                break;
            case "overtemperature":
                label.innerHTML = "Overtemperature: N/A";
                break;
            default:
                break;
        }
        listItem.appendChild(label);
        return listItem;
    };

    //Create list elements
    const power = createListItem("power");
    const energy = createListItem("energy");
    const temperature = createListItem("temperature");
    const status = createListItem("status");
    const overtemperature = createListItem("overtemperature");

    //Create toggle button and hide by default (we don't know if plug is on or off)
    const button = document.createElement("button");
    button.type = "button";
    button.id = `${plug_id}-button`;
    button.addEventListener("click", function() {
        toggleRelay(plug_id);

    });
    button.style.display = "none";

    //Append all the list entries on the unnumbered list
    list.append(power, energy, temperature, overtemperature, status, button);

    //Append headline and list to the div
    div.appendChild(headline);
    div.appendChild(list);

    //If the "no plugs found" label is still there, delete it as we found atleast one
    const noPlugs = document.getElementById("noPlugLabel");
    if (noPlugs) {
        noPlugs.remove();
    }
}