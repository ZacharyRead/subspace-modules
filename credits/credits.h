#ifndef CREDS_H_INCLUDED
#define CREDS_H_INCLUDED

#define CB_CREDITS "credits"
typedef void (*CreditFunc)(Player *p, int old);

#define I_CREDITS "credits-1"
typedef struct Icredits
{
    INTERFACE_HEAD_DECL

    /** Gets the number of credits a player has
     *
     * @param p, The player to get credits from.
     * @return the number of credits
     */
    unsigned long (*GetCredits)(Player *p);

    /** Sets the number of credits a player has
     *
     * @param p, The player to set credits.
     * @param creds, the number of credits
     */
    void (*SetCredits)(Player *p, unsigned long creds);

    /** Adds Credits.
     *
     * @param p, the player to give credits.
     * @param creds, the number of credits to add.
     */
    void (*AddCredits)(Player *p, unsigned long creds);

    /** Same as AddCredits, but instead of increase
     *  it will decrease the player's credits
     */
    void (*RemoveCredits)(Player *p, unsigned long creds);

    /** This saves a player's credits to the database
     *
     * @param p, The player to save.
     */
    void (*SavePlayer)(Player *p);

    /** Updates the database, should be called if
     *  you're going to shutdown the database or asss.
     */
    void (*UpdateDB)();
} Icredits;

#endif // CREDS_H_INCLUDED
