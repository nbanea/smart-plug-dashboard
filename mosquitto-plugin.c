#include <stdio.h>
#include <string.h>
#include <sqlite3.h>
#include <mosquitto.h>
#include <mosquitto_plugin.h>
#include <mosquitto_broker.h>

/**
 * Mosquitto plugin to log Shelly plugs messages into an SQLite database
 * SQLite database needs to exist and contain needed tables. Adapt to your needs if needed.
 * To compile: gcc -Wall  -shared -fPIC mosquitto-plugin.c -o mosquitto-plugin.so -lmosquitto -lsqlite3
 * For Ubuntu or other distributions with outdated mosquitto development files, you might require pointing gcc to updated include files.
 * You can obtain those by cloning the official Mosquitto repository and use the gcc flag -I path/to/include
 * In summary: gcc -Wall  -shared -fPIC mosquitto-plugin.c -o mosquitto-plugin.so -lmosquitto -lsqlite3 -I path/to/include
 * Made by: Nikolas Banea <nikolas.banea@studmail.htw-aalen.de>
*/

static sqlite3 *db;
static mosquitto_plugin_id_t *mosq_pid = NULL;

//Reason to ignore:             Posted on first connection, Fahrenheit temperature not relevant in EU, Overtemperature is subject to change, command as it's the publish one to toggle
//                              info also posted on first connection, don't need, announce not required to be logged
const char* topicsToIgnore[] = {"online", "temperature_f", "overtemperature", "command", "info", "announce"};

/**
 * Checks if the plug is already in DB. The plug with its id will be saved into "plugs" table if it isn't known yet.
 * @param plug_name Name of plug
*/
void checkIfPlugInDB(char* plug_name)
{
    //SQL Query with wildcard
    const char *selectQuery = "SELECT * FROM plugs WHERE plug_name = ?";

    sqlite3_stmt *stmt;
    //Prepare statement
    int rc = sqlite3_prepare_v2(db, selectQuery, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        mosquitto_log_printf(MOSQ_LOG_INFO, "Failed to prepare select query: %s", sqlite3_errmsg(db));
    }

    //Bind the plug name to the statement wildcard
    sqlite3_bind_text(stmt, 1, plug_name, -1, NULL);
    rc = sqlite3_step(stmt);

    //SQLITE_ROW means a row matching the query was found, in this case, a plug, therefore we just return
    if (rc == SQLITE_ROW) {
        mosquitto_log_printf(MOSQ_LOG_INFO, "Plug already exists in the table. Skipping insertion.");
        return;
    } 
    //SQLITE_DONE means it's done with the query, no rows found, therefore plug isn't known,
    //it is added to the table
    else if (rc == SQLITE_DONE) {
        const char *insertQuery = "INSERT INTO plugs (plug_name) VALUES (?)";

        //Prepare the statement
        rc = sqlite3_prepare_v2(db, insertQuery, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            mosquitto_log_printf(MOSQ_LOG_INFO, "Failed to prepare insert query: %s", sqlite3_errmsg(db));
        }

        //Bind the plug name to the wildcard
        sqlite3_bind_text(stmt, 1, plug_name, -1, NULL);
        rc = sqlite3_step(stmt);

        //If return code isn't DONE, it means there was an error
        if (rc != SQLITE_DONE) {
            mosquitto_log_printf(MOSQ_LOG_INFO, "Failed to insert plug: %s", sqlite3_errmsg(db));
        } else {
            mosquitto_log_printf(MOSQ_LOG_INFO, "Plug inserted successfully.");
        }
    } 
    //If not ROW or DONE were returned, it means there was a different issue
    else {
         mosquitto_log_printf(MOSQ_LOG_INFO, "Failed to execute select query: %s\n", sqlite3_errmsg(db));
    }

    //Destroy the stmt object
    sqlite3_finalize(stmt);
}

/**
 * Check if a string contains one of the substrings in an array
 * @param stringToCheck String to check for the substrings
 * @param subStrings Array of substrings
 * @param arrayLength Substring array length
 * @returns True if string contains substring, false otherwise
*/
bool containsString(const char* stringToCheck, const char* subStrings[], size_t arrayLength) {
    for (size_t i = 0; i < arrayLength; ++i) {
        if (strstr(stringToCheck, subStrings[i]) != NULL) {
            return true; 
        }
    }
    return false;
}

/**
 * Callback function, called on message. Logs the wanted values in the SQLite database.
*/
int callback_log(int event, void *event_data, void *userdata)
{
    const char *query;
    sqlite3_stmt *stmt;
    int rc;
    struct mosquitto_evt_message * ed = (struct mosquitto_evt_message *) event_data;
    char **topics;
 	int topic_count;
    //Divide topic into subtopics, easier to check each
 	mosquitto_sub_topic_tokenise(ed->topic, &topics, &topic_count);
    //Check if topic contains ignored topics
    if(containsString(ed->topic,topicsToIgnore,sizeof(topicsToIgnore)/sizeof(topicsToIgnore[0]))){
        mosquitto_log_printf(MOSQ_LOG_INFO, "Not important topic, skipping");
        return MOSQ_ERR_SUCCESS;
    }

    //Shellies always have their ID on the 2nd level, unless it's announce or online
    char* shelly_id = topics[1];

    //Check if plug is already known
    checkIfPlugInDB(shelly_id);

    mosquitto_log_printf(MOSQ_LOG_INFO, "Topic: %s", ed->topic);

    //Temperature is an odd topic, it's on the 3rd level unlike the other topics
    if((strcmp(topics[2], "temperature")) == 0)
    {
        query = "INSERT INTO plug_temperature (plug_name, plug_temperature) VALUES(? ,?)";
    }
    //Status is on /shellies/<id>/relay/0
    else if(topic_count == 4 && (strcmp(topics[3], "0")) == 0)
    {
        query = "INSERT INTO plug_status (plug_name, plug_status) VALUES(? ,?)";
    }
    //Power and energy are after .../relay/0/<attribute>
    else if(topic_count == 5 && (strcmp(topics[4], "power")) == 0)
    {
        query = "INSERT INTO plug_power (plug_name, plug_power) VALUES(? ,?)";
    }
    else if(topic_count == 5 && (strcmp(topics[4], "energy")) == 0)
    {
        query = "INSERT INTO plug_consumption (plug_name, plug_consumption) VALUES(? ,?)";
    }

    //Prepare the statement
    rc = sqlite3_prepare_v2(db, query, strlen(query), &stmt, NULL);
    if (rc != SQLITE_OK) {
        mosquitto_log_printf(MOSQ_LOG_INFO, "Error preparing statement: %s\n", sqlite3_errmsg(db));
        return MOSQ_ERR_UNKNOWN;
    }

    //Bind shelly id to plug_name
    rc = sqlite3_bind_text(stmt, 1, shelly_id, strlen(shelly_id), SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        mosquitto_log_printf(MOSQ_LOG_INFO, "Error binding parameter: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return MOSQ_ERR_UNKNOWN;
    }

    //Bind message to plug_<attribute>
    rc = sqlite3_bind_text(stmt, 2, ed->payload, ed->payloadlen, SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        mosquitto_log_printf(MOSQ_LOG_INFO, "Error binding parameter: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return MOSQ_ERR_UNKNOWN;
    }

    //Execute statement
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        mosquitto_log_printf(MOSQ_LOG_INFO, "Error executing statement: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return MOSQ_ERR_UNKNOWN;
    }

    //Delete statement
    sqlite3_finalize(stmt);
    return MOSQ_ERR_SUCCESS;
}

int mosquitto_plugin_version(int n, const int * v)
{
	int i = -1;;
	while(++i < n && v[i] != 5);
	return (i < n) ? 5 : -1;
}

int mosquitto_plugin_init(mosquitto_plugin_id_t * pid, void **user_data, struct mosquitto_opt *opts, int opt_count)
{
    //Path to SQLite database file, make sure rights to the database are enough for mosquitto to access
    const char *db_file = "path/to/db.sqlite";

    //Return code
    int rc;

    mosq_pid = pid;

    //Open database and make sure it successfully was opened
    rc = sqlite3_open(db_file, &db);
    if (rc != SQLITE_OK) {
         mosquitto_log_printf(MOSQ_LOG_INFO, "Error opening database: %s\n", sqlite3_errmsg(db));
        return MOSQ_ERR_UNKNOWN;
    }
    mosquitto_log_printf(MOSQ_LOG_INFO, "Opened DB");
    
    //Register the logging callback to the message event
    mosquitto_callback_register(mosq_pid, MOSQ_EVT_MESSAGE, callback_log, NULL, *user_data);

    return MOSQ_ERR_SUCCESS;
}

int mosquitto_plugin_cleanup(void *user_data, struct mosquitto_opt *opts, int opt_count)
{
	mosquitto_log_printf(MOSQ_LOG_INFO, "Shutting down");
    mosquitto_callback_unregister(mosq_pid, MOSQ_EVT_MESSAGE, callback_log, NULL);
	sqlite3_close(db);
    return MOSQ_ERR_SUCCESS;
}