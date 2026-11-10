#!/usr/bin/env python3
"""Maintenance helper for the Hebrew catalogue.

zionos_he.ts is the source of truth that lrelease compiles. This script fills
entries that are still unfinished from the table below (after `lupdate` added
new strings) and reports anything left untranslated. Usage:
    python3 fill_he.py [zionos_he.ts]
"""
import sys
import xml.etree.ElementTree as ET

LRI, PDI = "⁦", "⁩"

HE = {
"unknown": "לא ידוע",
"Version": "גרסה",
"Based on": "מבוסס על",
"Kernel": "ליבה",
"Architecture": "ארכיטקטורה",
"Desktop": "שולחן עבודה",
"Session": "הפעלה",
"Live session (not installed)": "הפעלה חיה (לא מותקן)",
"Installed system": "מערכת מותקנת",
"ZionOS is a community distribution built on Debian. Security updates for the base system are provided by the Debian project through the normal update tools. Documentation is installed in /usr/share/doc/zionos-base.":
    "ZionOS היא הפצה קהילתית המבוססת על Debian. עדכוני האבטחה של מערכת הבסיס מסופקים על ידי פרויקט Debian באמצעות כלי העדכון הרגילים. התיעוד מותקן בנתיב " + LRI + "/usr/share/doc/zionos-base" + PDI + ".",
"ZionOS is not affiliated with or endorsed by the Debian Project. Debian is a registered trademark of Software in the Public Interest, Inc.":
    "ZionOS אינה קשורה לפרויקט Debian ואינה נתמכת על ידו. Debian הוא סימן מסחרי רשום של Software in the Public Interest, Inc.",
"Copy details": "העתקת הפרטים",
"The assistant is optional. It works only after you add your own API key for a provider below. When you send a message, the conversation text and anything you explicitly attach are sent to the selected provider over an encrypted connection. Nothing is sent in the background, and usage is billed by the provider to your account.":
    "העוזר הוא אופציונלי. הוא פועל רק לאחר הוספת מפתח API משלך לאחד הספקים שלהלן. בעת שליחת הודעה, טקסט השיחה וכל מה שצירפת במפורש נשלחים לספק שנבחר בחיבור מוצפן. דבר אינו נשלח ברקע, והשימוש מחויב על ידי הספק בחשבון שלך.",
"Default provider:": "ספק ברירת המחדל:",
"Conversation history": "היסטוריית שיחות",
"Save conversations on this computer": "שמירת שיחות במחשב זה",
"History is stored only in your home folder: %1": "ההיסטוריה נשמרת רק בתיקיית הבית שלך: %1",
"Delete all saved conversations…": "מחיקת כל השיחות השמורות…",
"Delete history": "מחיקת היסטוריה",
"Delete all saved conversations from this computer? This cannot be undone.": "למחוק את כל השיחות השמורות ממחשב זה? לא ניתן לבטל פעולה זו.",
"%n conversation(s) deleted.": ["שיחה אחת נמחקה.", "שתי שיחות נמחקו.", "%n שיחות נמחקו."],
"Privacy and limits": "פרטיות ומגבלות",
"Allow OpenAI to store responses on its servers (about 30 days)": "לאפשר ל־OpenAI לשמור תשובות בשרתיה (כ־30 יום)",
"If Anthropic declines a request, let it answer with its fallback model": "אם Anthropic דוחה בקשה, לאפשר לה לענות באמצעות מודל הגיבוי שלה",
"Uses Anthropic's server-side fallback. The reply is then billed at the fallback model's rates.": "שימוש במנגנון הגיבוי בצד השרת של Anthropic. התשובה מחויבת אז לפי תעריפי מודל הגיבוי.",
"Maximum reply length (tokens):": "אורך תשובה מרבי (טוקנים):",
"s": "שנ׳",
"Give up when no data arrives for:": "לוותר כאשר לא מגיעים נתונים במשך:",
"API key:": "מפתח API:",
"Paste a new key here": "יש להדביק כאן מפתח חדש",
"Save key": "שמירת המפתח",
"Remove key": "הסרת המפתח",
"Refresh list": "רענון הרשימה",
"Ask the provider which models your key can use": "בירור מול הספק אילו מודלים זמינים למפתח שלך",
"Model:": "מודל:",
"Data is sent to: %1": "הנתונים נשלחים אל: %1",
"A key is held in memory for this session only. It will be forgotten when the assistant closes.": "מפתח מוחזק בזיכרון להפעלה זו בלבד. הוא יישכח עם סגירת העוזר.",
"Checking the keyring…": "בודק את מחזיק המפתחות…",
"A key is saved in the system keyring (encrypted).": "מפתח שמור במחזיק המפתחות של המערכת (מוצפן).",
"The system keyring is not available or is locked: %1": "מחזיק המפתחות של המערכת אינו זמין או נעול: %1",
"No key saved.": "לא נשמר מפתח.",
"Held for this session only (not saved).": "מוחזק להפעלה זו בלבד (לא נשמר).",
"The system keyring is not available or is locked.": "מחזיק המפתחות של המערכת אינו זמין או נעול.",
"Keyring unavailable": "מחזיק המפתחות אינו זמין",
"The key could not be saved because the system keyring (KDE Wallet / Secret Service) is not available or is locked.": "לא ניתן היה לשמור את המפתח מפני שמחזיק המפתחות של המערכת (KDE Wallet / Secret Service) אינו זמין או נעול.",
"ZionOS never stores API keys in plain text. To save the key permanently, open KDE Wallet Manager, enable or unlock the wallet, and try again.": "ZionOS לעולם אינה שומרת מפתחות API כטקסט גלוי. כדי לשמור את המפתח באופן קבוע, יש לפתוח את מנהל הארנקים של KDE, להפעיל את הארנק או לבטל את נעילתו, ולנסות שוב.",
"Use for this session only": "שימוש בהפעלה זו בלבד",
"Open Wallet Manager": "פתיחת מנהל הארנקים",
"Remove the saved API key for %1 from this computer?": "להסיר ממחשב זה את מפתח ה־API השמור עבור %1?",
"The key could not be removed from the keyring: %1": "לא ניתן היה להסיר את המפתח ממחזיק המפתחות: %1",
"Save an API key first.": "יש לשמור תחילה מפתח API.",
"The model list could not be loaded (%1).": "לא ניתן היה לטעון את רשימת המודלים (%1).",
"Stopped.": "נעצר.",
"Could not reach %1. Check your Internet connection and try again.": "לא ניתן היה להתחבר אל %1. יש לבדוק את החיבור לאינטרנט ולנסות שוב.",
"%1 did not respond in time, so the request was stopped.": "לא התקבלה תגובה מ־%1 בזמן, ולכן הבקשה נעצרה.",
"%1 rejected the API key. Check the key, or replace it in the assistant settings.": "המפתח נדחה על ידי %1. יש לבדוק את המפתח או להחליף אותו בהגדרות העוזר.",
"%1 refused the request for this account. This usually means missing permissions, a billing problem or no remaining credit.": "הבקשה סורבה על ידי %1 עבור חשבון זה. בדרך כלל פירוש הדבר הרשאות חסרות, בעיית חיוב או יתרה שאזלה.",
"%1 is limiting requests from your account right now.": "כרגע %1 מגביל את קצב הבקשות מהחשבון שלך.",
"The provider asks you to wait %n second(s) before trying again.": ["הספק מבקש להמתין שנייה אחת לפני ניסיון נוסף.", "הספק מבקש להמתין שתי שניות לפני ניסיון נוסף.", "הספק מבקש להמתין %n שניות לפני ניסיון נוסף."],
"%1 is temporarily overloaded. Try again in a little while.": "השירות של %1 עמוס זמנית. יש לנסות שוב בעוד זמן קצר.",
"%1 did not accept the request.": "הבקשה לא התקבלה על ידי %1.",
"%1 reported an internal error. Try again later.": "התקבל דיווח על שגיאה פנימית מ־%1. יש לנסות שוב מאוחר יותר.",
"The reply from %1 was incomplete or could not be understood.": "התשובה מ־%1 הייתה חלקית או שלא ניתן היה לפענח אותה.",
"The request was not sent again automatically.": "הבקשה לא נשלחה שוב באופן אוטומטי.",
"Message from the provider: %1": "הודעה מהספק: %1",
"The assistant does not run as the administrator (root). Start it as a normal user.": "העוזר אינו פועל כמנהל המערכת (root). יש להפעיל אותו כמשתמש רגיל.",
"You": "את/ה",
"Assistant · %1": "העוזר · %1",
"Waiting for the reply…": "ממתין לתשובה…",
"Waiting for the system keyring… If a KDE Wallet window is open, answer it, or press Stop.": "ממתין למחזיק המפתחות של המערכת… אם נפתח חלון של KDE Wallet, יש להשיב לו, או ללחוץ על ״עצירה״.",
"ZionOS Assistant": "העוזר של ZionOS",
"Assistant": "העוזר",
"New chat": "שיחה חדשה",
"Provider:": "ספק:",
"Settings and API keys": "הגדרות ומפתחות API",
"Saved conversations": "שיחות שמורות",
"History is turned off. Conversations are not saved.": "ההיסטוריה כבויה. שיחות אינן נשמרות.",
"Delete selected": "מחיקת השיחה שנבחרה",
"Ask a question… (Ctrl+Enter to send)": "אפשר לשאול שאלה… (Ctrl+Enter לשליחה)",
"Attach file…": "צירוף קובץ…",
"Attach clipboard text": "צירוף טקסט מלוח הגזירים",
"Review what will be sent…": "סקירת מה שיישלח…",
"Stop": "עצירה",
"Send": "שליחה",
"When you press Send, this conversation and anything you attached are sent to %1 (%2). Nothing else on this computer is shared, and nothing is sent in the background.": "בלחיצה על ״שליחה״, שיחה זו וכל מה שצירפת נשלחים אל %1 (%2). שום דבר אחר במחשב זה אינו משותף, ודבר אינו נשלח ברקע.",
"Ask about ZionOS, paste an error message to have it explained, draft or translate text, or ask for a terminal command.\n\nThe assistant only suggests commands — it never runs them. Suggested commands can be wrong: read them before you use them.": "אפשר לשאול על ZionOS, להדביק הודעת שגיאה כדי לקבל הסבר, לנסח או לתרגם טקסט, או לבקש פקודת טרמינל.\n\nהעוזר רק מציע פקודות — הוא לעולם אינו מריץ אותן. פקודות מוצעות עלולות להיות שגויות: יש לקרוא אותן לפני השימוש.",
"(untitled)": "(ללא כותרת)",
"%1 (%2) — remove": "%1 (%2) — הסרה",
"Attach a text file": "צירוף קובץ טקסט",
"The file could not be read.": "לא ניתן היה לקרוא את הקובץ.",
"The file is larger than %1. Attach a smaller file or paste the relevant part.": "הקובץ גדול מ־%1. יש לצרף קובץ קטן יותר או להדביק את החלק הרלוונטי.",
"Only plain text files (UTF-8) can be attached.": "ניתן לצרף רק קובצי טקסט פשוט (UTF-8).",
"Attach file": "צירוף קובץ",
"Clipboard text": "טקסט מלוח הגזירים",
"Selected text": "טקסט מסומן",
"The clipboard does not contain any text.": "לוח הגזירים אינו מכיל טקסט.",
"The clipboard text is too large to attach.": "הטקסט שבלוח הגזירים גדול מכדי לצרפו.",
"Review what will be sent": "סקירת מה שיישלח",
"The following will be sent to %1 (%2) using the model %3. Your API key is sent as a request header and is not shown here.": "התוכן שלהלן יישלח אל %1 (%2) באמצעות המודל %3. מפתח ה־API שלך נשלח ככותרת של הבקשה ואינו מוצג כאן.",
"Instructions to the assistant": "הנחיות לעוזר",
"Total request size: %1": "גודל הבקשה הכולל: %1",
"(The reply was empty.)": "(התשובה הייתה ריקה.)",
"The reply was cut off because it reached the maximum length.": "התשובה נקטעה מפני שהגיעה לאורך המרבי.",
"The provider declined to answer this request.": "הספק סירב לענות על בקשה זו.",
"Answered by model %1.": "נענה על ידי המודל %1.",
"Tokens reported by the provider — input: %1, output: %2, total: %3": "טוקנים כפי שדווחו על ידי הספק — קלט: %1, פלט: %2, סך הכול: %3",
"This reply is incomplete.": "תשובה זו אינה שלמה.",
"API key needed": "נדרש מפתח API",
"The system keyring could not be opened, so the API key for %1 is not available. Unlock the keyring (KDE Wallet), or enter the key for this session only. Nothing was sent.": "לא ניתן היה לפתוח את מחזיק המפתחות של המערכת, ולכן מפתח ה־API עבור %1 אינו זמין. יש לבטל את נעילת מחזיק המפתחות (KDE Wallet) או להזין את המפתח להפעלה זו בלבד. דבר לא נשלח.",
"No API key is saved for %1. Add your own key to use the assistant. Nothing was sent.": "לא שמור מפתח API עבור %1. יש להוסיף מפתח משלך כדי להשתמש בעוזר. דבר לא נשלח.",
"Open assistant settings": "פתיחת הגדרות העוזר",
"Assistant settings": "הגדרות העוזר",
"%n character(s)": ["תו אחד", "שני תווים", "%n תווים"],
"Proposed command — it has not been run. Read the explanation above and make sure you understand it before pasting it into a terminal.": "פקודה מוצעת — היא לא הורצה. יש לקרוא את ההסבר שלמעלה ולוודא שהיא מובנת לך לפני הדבקתה בטרמינל.",
"Copy command": "העתקת הפקודה",
"Copy code": "העתקת הקוד",
"Copied": "הועתק",
"Link blocked": "הקישור נחסם",
"This link was not opened because it does not point to a web page:\n%1": "קישור זה לא נפתח מפני שאינו מוביל לדף אינטרנט:\n%1",
"Open link?": "לפתוח את הקישור?",
"The assistant's reply contains a link. Open it in your web browser?": "תשובת העוזר מכילה קישור. לפתוח אותו בדפדפן האינטרנט?",
"Open": "פתיחה",
"ZionOS Settings": "הגדרות ZionOS",
"Language": "שפה",
"Keyboard": "מקלדת",
"System": "מערכת",
"Software": "תוכנה",
"About": "על אודות",
"Choose the language of the desktop and of ZionOS applications.": "בחירת השפה של שולחן העבודה ושל יישומי ZionOS.",
"Apply language": "החלת השפה",
"The desktop language setting could not be saved.": "לא ניתן היה לשמור את הגדרת שפת שולחן העבודה.",
"ZionOS applications use the new language the next time they start. The rest of the desktop follows after you log out and back in.": "יישומי ZionOS ישתמשו בשפה החדשה בהפעלתם הבאה. שאר שולחן העבודה יתעדכן לאחר יציאה מהחשבון וכניסה מחדש.",
"Hebrew is displayed right to left. Some programs that come from other projects are only partly translated to Hebrew; untranslated parts appear in English.": "עברית מוצגת מימין לשמאל. חלק מהתוכנות שמקורן בפרויקטים אחרים מתורגמות לעברית באופן חלקי בלבד; חלקים שלא תורגמו מופיעים באנגלית.",
"Region and formats (System Settings)…": "אזור ותבניות (הגדרות המערכת)…",
"Alt + Shift": "Alt + Shift",
"Meta (Windows key) + Space": "Meta (מקש Windows) + רווח",
"Ctrl + Shift": "Ctrl + Shift",
"Caps Lock": "Caps Lock",
"Alt + Space": "Alt + רווח",
"Keyboard layouts": "פריסות מקלדת",
"English (US)": "אנגלית (ארה״ב)",
"Hebrew": "עברית",
"No shortcut": "ללא קיצור",
"Switch layout with:": "החלפת פריסה באמצעות:",
"Apply": "החלה",
"More layouts and options (System Settings)…": "פריסות ואפשרויות נוספות (הגדרות המערכת)…",
"Select at least one layout.": "יש לבחור לפחות פריסה אחת.",
"Keyboard settings saved.": "הגדרות המקלדת נשמרו.",
"The keyboard settings could not be saved.": "לא ניתן היה לשמור את הגדרות המקלדת.",
"These settings are provided by KDE Plasma. Each button opens the matching page of System Settings.": "הגדרות אלה מסופקות על ידי KDE Plasma. כל כפתור פותח את העמוד המתאים בהגדרות המערכת.",
"Network and Wi-Fi": "רשת ו־Wi-Fi",
"Sound": "שמע",
"Displays": "צגים",
"Date and time": "תאריך ושעה",
"Users": "משתמשים",
"Appearance": "מראה",
"Power": "צריכת חשמל",
"Bluetooth": "Bluetooth",
"Printers": "מדפסות",
"Accessibility": "נגישות",
"System Settings could not be started.": "לא ניתן היה להפעיל את הגדרות המערכת.",
"Open System Settings": "פתיחת הגדרות המערכת",
"ZionOS installs software from the Debian archive with APT. Security updates come directly from Debian. Use Discover to browse, install and update software.": "ZionOS מתקינה תוכנה מארכיון Debian באמצעות APT. עדכוני האבטחה מגיעים ישירות מ־Debian. אפשר להשתמש ב־Discover כדי לעיין בתוכנות, להתקין ולעדכן אותן.",
"Open Discover": "פתיחת Discover",
"Check for updates": "בדיקת עדכונים",
"Flatpak applications (optional)": "יישומי Flatpak (אופציונלי)",
"Flatpak can install additional applications that are not packaged by Debian. Flathub is a third-party repository: its applications are not reviewed by Debian or ZionOS. It is not enabled unless you choose to add it. It is added for your user account only.": "Flatpak מאפשר להתקין יישומים נוספים שאינם ארוזים על ידי Debian. ‏Flathub הוא מאגר של צד שלישי: היישומים שבו אינם נבדקים על ידי Debian או ZionOS. הוא אינו מופעל אלא אם בחרת להוסיף אותו, והוא מתווסף לחשבון המשתמש שלך בלבד.",
"Add the Flathub repository": "הוספת המאגר Flathub",
"Remove the Flathub repository": "הסרת המאגר Flathub",
"Flatpak is not installed.": "Flatpak אינו מותקן.",
"Flathub is enabled for your account.": "Flathub מופעל בחשבון שלך.",
"Flathub is not enabled.": "Flathub אינו מופעל.",
"The Flatpak command failed:\n%1": "פקודת Flatpak נכשלה:\n%1",
"Welcome to ZionOS": "ברוכים הבאים ל־ZionOS",
"%1 — a Debian-based desktop in English and Hebrew.": "%1 — שולחן עבודה מבוסס Debian בעברית ובאנגלית.",
"Language:": "שפה:",
"Not installed": "לא מותקן",
"Install ZionOS": "התקנת ZionOS",
"Install to this computer's disk. You are running the live system now.": "התקנה לדיסק של מחשב זה. כרגע פועלת המערכת החיה.",
"Install software": "התקנת תוכנה",
"Browse and install applications and updates.": "עיון ביישומים והתקנת יישומים ועדכונים.",
"Language, keyboard layouts, assistant and system information.": "שפה, פריסות מקלדת, העוזר ומידע על המערכת.",
"System Settings": "הגדרות המערכת",
"Network, sound, displays, date and time, and everything else.": "רשת, שמע, צגים, תאריך ושעה וכל השאר.",
"Optional AI helper. Needs your own OpenAI or Anthropic API key.": "עוזר בינה מלאכותית אופציונלי. נדרש מפתח API משלך של OpenAI או Anthropic.",
"Files": "קבצים",
"Open your home folder.": "פתיחת תיקיית הבית.",
"About ZionOS": "על אודות ZionOS",
"Version, base system and support information.": "גרסה, מערכת הבסיס ומידע על תמיכה.",
"Live session: user “%1”, password “%2”. Changes are lost when you shut down.": "הפעלה חיה: משתמש ״%1״, סיסמה ״%2״. שינויים אובדים בעת כיבוי המחשב.",
"Show this window when I log in": "להציג חלון זה בעת הכניסה למערכת",
"Close": "סגירה",
"%1 could not be started.": "לא ניתן היה להפעיל את %1.",
}


RLM = "\u200f"


def rtl_base(text):
    """Make a Hebrew sentence that starts with a Latin word (a product name,
    a %1 placeholder) a right-to-left paragraph by prefixing U+200F RIGHT-TO-LEFT
    MARK. Without it the paragraph direction follows the first strong
    character and the whole sentence is laid out left-to-right."""
    import unicodedata
    if not any("\u0590" <= ch <= "\u05ff" for ch in text) or text.startswith(RLM):
        return text
    for ch in text:
        if ch == "%":            # placeholder: content unknown, usually Latin
            return RLM + text
        d = unicodedata.bidirectional(ch)
        if d == "L":
            return RLM + text
        if d in ("R", "AL"):
            return text
    return text


def main(path):
    tree = ET.parse(path)
    missing = []
    for msg in tree.getroot().iter("message"):
        src = msg.find("source").text
        tr = msg.find("translation")
        if src not in HE:
            if tr.get("type") == "unfinished":
                missing.append(src)
            continue
        val = HE[src]
        if msg.get("numerus") == "yes":
            forms = tr.findall("numerusform")
            n = len(forms) or 2
            for f in forms:
                tr.remove(f)
            one, two, many = val
            texts = {1: [many], 2: [one, many], 3: [one, two, many], 4: [one, two, many, many]}[n]
            for t in texts:
                ET.SubElement(tr, "numerusform").text = rtl_base(t)
        else:
            tr.text = rtl_base(val)
        tr.attrib.pop("type", None)
    ET.indent(tree, space="    ")
    with open(path, "wb") as f:
        f.write(b'<?xml version="1.0" encoding="utf-8"?>\n<!DOCTYPE TS>\n')
        tree.write(f, encoding="utf-8", xml_declaration=False)
        f.write(b"\n")
    if missing:
        print("UNTRANSLATED:")
        for m in missing:
            print("  ", repr(m))
        return 1
    print("Hebrew catalogue complete.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else __file__.replace("fill_he.py", "zionos_he.ts")))
