document.addEventListener('DOMContentLoaded', function() {
    // DOM elemanları
    const feedingSchedulesContainer = document.getElementById('feeding-schedules');
    const feedingTemplate = document.getElementById('feeding-template');
    const addFeedingBtn = document.getElementById('add-feeding-btn');
    const saveSchedulesBtn = document.getElementById('save-schedules-btn');
    const manualFeedBtn = document.getElementById('manual-feed-btn');
    const manualFeedDuration = document.getElementById('manual-feed-duration');
    const statusMessage = document.getElementById('status-message');
    
    // Maksimum besleme sayısı
    const MAX_FEEDINGS = 5;
    
    // Sayfa yüklendiğinde besleme programlarını getir
    fetchFeedingSchedules();
    
    // Besleme programlarını getir
    function fetchFeedingSchedules() {
        fetch('/settings')
            .then(response => response.json())
            .then(data => {
                // Mevcut besleme programlarını temizle
                feedingSchedulesContainer.innerHTML = '';
                
                // Besleme programlarını ekle
                data.feedings.forEach((feeding, index) => {
                    addFeedingSchedule(index + 1, feeding);
                });
                
                updateFeedingCount();
            })
            .catch(error => {
                console.error('Besleme programları yüklenirken hata oluştu:', error);
                showStatus('Besleme programları yüklenirken hata oluştu!', false);
            });
    }
    
    // Yeni besleme programı ekle
    function addFeedingSchedule(number, feeding = null) {
        // Template'i klonla
        const feedingElement = document.importNode(feedingTemplate.content, true);
        
        // Besleme numarasını ayarla
        feedingElement.querySelector('.feeding-number').textContent = number;
        
        // Eğer mevcut bir besleme programı varsa, değerleri ayarla
        if (feeding) {
            feedingElement.querySelector('.feeding-hour').value = feeding.hour;
            feedingElement.querySelector('.feeding-minute').value = feeding.minute;
            feedingElement.querySelector('.feeding-duration').value = feeding.duration;
            feedingElement.querySelector('.feeding-enabled').checked = feeding.enabled;
        }
        
        // Besleme programı silme butonu
        const removeBtn = feedingElement.querySelector('.remove-feeding-btn');
        removeBtn.addEventListener('click', function(e) {
            e.target.closest('.feeding-schedule').remove();
            updateFeedingCount();
        });
        
        // Besleme programını sayfaya ekle
        feedingSchedulesContainer.appendChild(feedingElement);
    }
    
    // Besleme sayısını güncelle ve "Yeni Besleme Ekle" butonunu kontrol et
    function updateFeedingCount() {
        const currentCount = document.querySelectorAll('.feeding-schedule').length;
        addFeedingBtn.disabled = currentCount >= MAX_FEEDINGS;
    }
    
    // Yeni besleme programı ekle butonu
    addFeedingBtn.addEventListener('click', function() {
        const currentCount = document.querySelectorAll('.feeding-schedule').length;
        
        if (currentCount < MAX_FEEDINGS) {
            addFeedingSchedule(currentCount + 1);
            updateFeedingCount();
        }
    });
    
    // Besleme programlarını kaydet butonu
    saveSchedulesBtn.addEventListener('click', function() {
        const feedingElements = document.querySelectorAll('.feeding-schedule');
        const feedings = [];
        
        feedingElements.forEach(element => {
            feedings.push({
                hour: parseInt(element.querySelector('.feeding-hour').value),
                minute: parseInt(element.querySelector('.feeding-minute').value),
                duration: parseInt(element.querySelector('.feeding-duration').value),
                enabled: element.querySelector('.feeding-enabled').checked
            });
        });
        
        // Eksik besleme programlarını varsayılan değerlerle doldur
        while (feedings.length < MAX_FEEDINGS) {
            feedings.push({
                hour: 0,
                minute: 0,
                duration: 500,
                enabled: false
            });
        }
        
        // Besleme programlarını kaydet
        saveFeedingSchedules(feedings);
    });
    
    // Besleme programlarını kaydet
    function saveFeedingSchedules(feedings) {
        fetch('/settings', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ feedings: feedings })
        })
        .then(response => response.json())
        .then(data => {
            if (data.status === 'success') {
                showStatus('Besleme programları başarıyla kaydedildi!', true);
            } else {
                showStatus('Besleme programları kaydedilirken hata oluştu!', false);
            }
        })
        .catch(error => {
            console.error('Besleme programları kaydedilirken hata oluştu:', error);
            showStatus('Besleme programları kaydedilirken hata oluştu!', false);
        });
    }
    
    // Manuel besleme butonu
    manualFeedBtn.addEventListener('click', function() {
        const duration = manualFeedDuration.value;
        
        manualFeedBtn.disabled = true;
        manualFeedBtn.textContent = 'Besleniyor...';
        
        fetch('/feed', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/x-www-form-urlencoded'
            },
            body: `duration=${duration}`
        })
        .then(response => response.json())
        .then(data => {
            if (data.status === 'success') {
                showStatus('Manuel besleme başarıyla gerçekleştirildi!', true);
            } else {
                showStatus('Manuel besleme sırasında hata oluştu!', false);
            }
        })
        .catch(error => {
            console.error('Manuel besleme sırasında hata oluştu:', error);
            showStatus('Manuel besleme sırasında hata oluştu!', false);
        })
        .finally(() => {
            manualFeedBtn.disabled = false;
            manualFeedBtn.textContent = 'Şimdi Besle';
        });
    });
    
    // Durum mesajını göster
    function showStatus(message, isSuccess) {
        statusMessage.textContent = message;
        statusMessage.className = isSuccess ? 'success' : 'error';
        
        // 3 saniye sonra mesajı temizle
        setTimeout(() => {
            statusMessage.textContent = '';
            statusMessage.className = '';
        }, 3000);
    }
}); 