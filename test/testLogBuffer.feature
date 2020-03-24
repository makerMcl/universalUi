Feature: log buffer

    Background: size of log buffer is 16 [characters]

    Scenario: empty buffer
        Given log buf is filled with ""
        When message "" is appended
        Then getLog() returns ""

    Scenario: simple logging
        Given log buf is filled with "abcd"
        When  message "xyz" is appended
        Then  getLog() returns "abcdxyz"

    Scenario: message larger than buf
        Given log buf is filled with ""
        When message "123456789012345678" is appended
        Then getLog() returns "[...] 9012345678"

    Scenario: roll-over of buffer
        Given log buf is filled with "1234567890"
        When message "abcdefghij" is appended
        # note: '5' is replaced by terminating \0 of message resulting in only 15 chars
        Then getLog() returns "67890abcdefghij"

    Scenario: print with line endings
        Given log buf is filled with "123456789\n"
        When message "abcde\n" is appended
        Then getLog() returns "123456789\nabcde\n"
