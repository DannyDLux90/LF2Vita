/* Public stock-data helpers consumed outside game.c. Keep this mapping identical
   to the runtime's lf2_id_to_roster() table in game_18.inc. */
int lf2_roster_from_original_id(int id){
    switch(id){
        case 1:return 0;case 2:return 1;case 4:return 2;case 5:return 3;case 6:return 4;
        case 7:return 5;case 8:return 6;case 9:return 7;case 10:return 8;case 11:return 9;
        case 30:return 10;case 31:return 11;case 32:return 12;case 33:return 13;case 34:return 14;
        case 35:return 15;case 36:return 16;case 37:return 17;case 38:return 19;case 39:return 18;
        case 50:return 20;case 51:return 21;case 52:return 22;default:return -1;
    }
}
