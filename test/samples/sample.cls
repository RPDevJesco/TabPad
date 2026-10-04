// an Apex service
public with sharing class AccountService {
    private static final Integer LIMIT_ROWS = 50;

    @AuraEnabled(cacheable=true)
    public static List<Account> recent(String name) {
        List<Account> rows = [SELECT Id, Name FROM Account WHERE Name LIKE :name LIMIT 50];

        for (Account a : rows) {
            System.debug('found ' + a.Name);
        }

        return rows;
    }
}
